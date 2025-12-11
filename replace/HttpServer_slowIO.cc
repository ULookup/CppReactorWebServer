#include "HttpServer_slowIO.h"
#include <spdlog/spdlog.h>

namespace webserver::server
{
/* brief: 创建服务器，并将消息处理函数绑定到server里 */
HttpServer::HttpServer(uint16_t port, int timeout, int worker_threads) : _server(port), _worker_pool(std::make_unique<http::ThreadPool>(worker_threads)) {
    _server.EnableInactiveRelease(timeout);
    _server.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1)); // 将OnConnection的this绑定后，剩下的Connection参数交给服务器去绑定
    _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));

    //初始化常用方法的根节点
    _roots["GET"] = std::make_shared<TrieNode>();
    _roots["POST"] = std::make_shared<TrieNode>();
    _roots["PUT"] = std::make_shared<TrieNode>();
    _roots["DELETE"] = std::make_shared<TrieNode>();
}
/* brief: 提供给使用者注册基准路径 */
void HttpServer::SetBaseDir(const std::string &path) {
    bool ret = util::Util::IsDirectory(path);
    assert(ret == true);
    _basedir = path;
}
// ============= Private ============
/* brief: 错误处理函数 */
void HttpServer::ErrorHandler(const http::HttpRequest &request, http::HttpResponse *response) {
    // step 1. 组织错误展示页面
    std::string filename;
    if(response->_status == 404) filename = _basedir + "404.html";
    else if(response->_status == 500) filename = _basedir + "500.html";
    else filename = _basedir + "error_default.html";

    std::string buf;
    bool ret = util::Util::ReadFile(filename, &buf);
    if(!ret) {
        std::string body;
            body += "<html>";
            body += "<head>";
            body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
            body += "</head>";
            body += "<body>";
            body += "<h1>";
            body += std::to_string(response->_status);
            body += " ";
            body += util::Util::StatusDesc(response->_status);
            body += "</h1>";
            body += "</body>";
            body += "</html>";

            response->SetContent(body, "text/html");
            return;
    }

    response->SetContent(buf, "text/html");
}
/* brief: 对应连接写入响应的函数 */
void HttpServer::WriteResponse(const std::shared_ptr<src::Connection> &connection, const http::HttpRequest &request, http::HttpResponse &response) {
    // 1.先完善头部字段
    if(request.IsClose() == true) response.SetHeader("Connection", "close");
    else response.SetHeader("Connection", "keep-alive");

    if(response._body.empty() == false && response.HasHeader("Content-Length") == false) {
        response.SetHeader("Content-Length", std::to_string(response._body.size()));
    }
    if(response._body.empty() == false && response.HasHeader("Content-Type") == false) {
        response.SetHeader("Content-Type", "application/octet-stream");
    }
    if(response._redirect_flag == true) {
        response.SetHeader("Location", response._redirect_url);
    }
    // 2.将response里的要素，按照http格式进行组织, 先组织 Header
    /* std::stringstream rsp_str;
    rsp_str << request._version << " " << std::to_string(response._status) << " " << util::Util::StatusDesc(response._status) << "\r\n";
    for(auto &head : response._headers) {
        rsp_str << head.first << ": " << head.second << "\r\n";
    }
    rsp_str << "\r\n";
    rsp_str << response._body;*/
    std::string header;
    header.reserve(256);

    header += request._version;
    header += " ";
    header += std::to_string(response._status);
    header += " ";
    header += util::Util::StatusDesc(response._status);
    header += "\r\n";

    for(auto &kv : response._headers) {
        if(kv.first.rfind("X-SENDFILE", 0) == 0) continue;
        header += kv.first + ": " + kv.second + "\r\n";
    }

    header += "\r\n";
    SPDLOG_TRACE("HttpResponse 的 Headers:");
    SPDLOG_TRACE("{}", header);

    // 2.发送Header
    SPDLOG_TRACE("调用Send, 发送响应头");
    connection->Send(header.c_str(), header.size());
    // 3 发送Body
    if(response.HasHeader("X-SENDFILE-FD")) {
        //静态资源，调用 SendFile
        //因为 Connection 保证先发Buffer后发File，所以顺序是安全的
        SPDLOG_TRACE("请求静态资源, 用SendFile实现零拷贝");
        int fd = std::stoi(response.GetHeader("X-SENDFILE-FD"));
        size_t size = std::stoul(response.GetHeader("X-SENDFILE-SIZE"));

        //获取 Offset, 默认为 0;
        off_t offset = 0;
        if(response.HasHeader("X-SENDFILE-OFFSET")) {
            offset = std::stoll(response.GetHeader("X-SENDFILE-OFFSET"));
        }
        connection->SendFile(fd, offset, size);
    } else if(!response._body.empty()) {
        SPDLOG_TRACE("请求普通资源, 调用Send发送body");
        connection->Send(response._body.c_str(), response._body.size());
    }
}
/* brief: 判断是不是静态资源请求 */
bool HttpServer::IsFileHandler(const http::HttpRequest &request) {
    SPDLOG_DEBUG("进入静态资源判断函数");
    // 1. 必须设置了静态资源根目录
    if(_basedir.empty()) return false;
    // 2. 请求方法必须是GET/HEAD
    if(request._method != "GET" && request._method != "HEAD") {
        SPDLOG_DEBUG("请求方法不符合静态资源");
        return false;
    }
    // 3. 请求的资源路径必须是一个合法路径
    if(util::Util::ValidPath(request._path) == false) {
        SPDLOG_WARN("资源路径不合法");
        return false;
    }
    // 4. 请求的资源必须存在，且是普通文件
    std::string request_path = _basedir + request._path; // 为了避免直接修改请求的资源路径
    if(request_path.back() == '/') request_path += "index.html";
    if(util::Util::IsRegular(request_path) == false) {
        SPDLOG_DEBUG("资源不是普通文件: {}", request_path);
        return false;
    }
    SPDLOG_DEBUG("请求的资源是静态资源，退出静态资源判断函数");
    return true;
}
/* brief: 静态资源处理函数 */
void HttpServer::FileHandler(const http::HttpRequest &request, http::HttpResponse *response) {
    SPDLOG_DEBUG("进入FileHandler函数");
    std::string request_path = _basedir + request._path; // 为了不破坏原始请求
    if(request_path.back() == '/') request_path += "index.html";
    SPDLOG_TRACE("request_path: {}", request_path);
    /* bool ret = util::Util::ReadFile(request_path, &response->_body); */
    //step1: 打开文件
    int fd = open(request_path.c_str(), O_RDONLY);
    if(fd < 0) {
        SPDLOG_ERROR("打开文件失败");
        response->_status = 404;
        ErrorHandler(request, response);
        return;
    }
    //step2: 获取文件状态
    struct stat st;
    if(fstat(fd, &st) < 0) {
        SPDLOG_ERROR("获取文件状态失败");
        close(fd);
        response->_status = 404;
        ErrorHandler(request, response);
        return;
    }
    size_t file_size = st.st_size;
    std::string mime = util::Util::ExtMime(request_path);

    //step3: 设置通用头部
    response->SetHeader("Content-Type", mime);
    response->SetHeader("Accept-Ranges", "bytes"); // 告诉浏览器支持断点续传/视频拖动

    //step3.1: 设置特殊头部
    response->SetHeader("Access-Control-Allow-Origin", "*");
    response->SetHeader("Access-Control-Allow-Methods", "GET, HEAD, OPTIONS");

    //ts 切片：强制缓存1年
    if(mime == "video/mp2t") { response->SetHeader("Cache-Control", "public, max-age=31536000, immutable"); }
    else if(mime == "application/vnd.apple.mpegurl") { response->SetHeader("Cache-Control", "no-cache"); } // m3u8索引，不缓存或者短缓存(防止更新了切片还在用旧索引)
    else { response->SetHeader("Cache-Control", "publicm max-age=3600"); } //其它静态资源默认缓存策略

    //step4: 检查 Range 头部
    if(request.HasHeader("Range")) {
        SPDLOG_DEBUG("该请求是Range请求");
        std::string_view range_val = request.GetHeaderView("Range");
        off_t start = 0;
        off_t end = file_size - 1;

        //解析 Range
        util::Util::ParseRange(range_val, file_size, start, end);

        //校验 Range 合法性
        if(start > end || start >= file_size) {
            SPDLOG_WARN("该Range请求不合法: Range: {}", range_val);
            response->_status = 416; //Range Not Satisfiable
            response->SetHeader("Content-Range", "bytes */" + std::to_string(file_size));
            close(fd);
            return;
        }
        SPDLOG_DEBUG("合法Range请求");
        size_t content_len = end - start + 1;

        response->_status = 206; // Partial Content
        response->SetHeader("Content-Length", std::to_string(content_len));
        std::string content_range = "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(file_size);
        response->SetHeader("Content-Range", content_range);
        SPDLOG_TRACE("构造Range响应: Content-Range: {}", content_range);
        //通过自定义头部传给WriteResponse
        response->_headers["X-SENDFILE-FD"] = std::to_string(fd);
        response->_headers["X-SENDFILE-SIZE"] = std::to_string(content_len);
        response->_headers["X-SENDFILE-OFFSET"] = std::to_string(start); // 传递偏移量
    } else {
        SPDLOG_DEBUG("该请求是非Range请求");
        response->_status = 200;
        response->SetHeader("Content-Length", std::to_string(file_size));

        response->_headers["X-SENDFILE-FD"] = std::to_string(fd);
        response->_headers["X-SENDFILE-SIZE"] = std::to_string(st.st_size);
        response->_headers["X-SENDFILE-OFFSET"] = "0"; // 默认偏移量0
    }
    
    // 判断 MIME 是否以 image/ 开头，如果是，就开启缓存
    /* if(mime.rfind("image/", 0) == 0) response->SetHeader("Cache-Control", "max-age=31536000, public"); */
    response->_body.clear();
    SPDLOG_DEBUG("退出FileHandler函数");
}
/* brief: 添加路由到 Trie */
void HttpServer::AddRoute(const std::string &method, const std::string &pattern, const Handler &handler) {
    if(_roots.find(method) == _roots.end()) {
        _roots[method] = std::make_shared<TrieNode>();
    }
    auto node = _roots[method];

    std::vector<std::string> parts = util::Util::SplitPath(pattern);
    for(const auto &part : parts) {
        if(!part.empty() && part[0] == ':') {
            // 处理动态参数，例如 :id
            if(!node->_param_child) {
                node->_param_child = std::make_shared<TrieNode>();
                node->_param_name = part.substr(1); // 存储"id";
            }
            // 简单实现: 如果有冲突的参数名，这里会覆盖，业务层应该避免
            node->_param_name = part.substr(1);
            node = node->_param_child;
        } else {
            // 处理静态路径
            if(node->_children.find(part) == node->_children.end()) {
                node->_children[part] = std::make_shared<TrieNode>();
            }
            node = node->_children[part];
        }
    }
    node->_is_end = true;
    node->_handler = handler;
    SPDLOG_DEBUG("注册路由: [{}] {}", method, pattern);
}
/* brief: 在 Trie 中匹配路由 */
bool HttpServer::MatchRoute(const std::string &method, const std::string &path, Handler &handler, std::unordered_map<std::string, std::string> &params) {
    if(_roots.find(method) == _roots.end()) return false;
    auto node = _roots[method];

    std::vector<std::string> parts = util::Util::SplitPath(path);

    for(const auto &part : parts) {
        // 优先匹配静态路径
        if(node->_children.count(part)) {
            node = node->_children[part];
        }
        // 匹配动态参数
        else if(node->_param_child) {
            //捕获参数
            params[node->_param_name] = part;
            node = node->_param_child;
        }
        // 匹配失败
        else {
            return false;
        }
    }

    if(node->_is_end && node->_handler) {
        handler = node->_handler;
        return true;
    }
    return false;
}
/* brief: 分发函数,动静分离 */
void HttpServer::Dispatcher(const std::shared_ptr<src::Connection> &connection, http::HttpRequest &request, http::HttpResponse *response) {
    //step1: 静态资源处理
    if(IsFileHandler(request)) {
        SPDLOG_DEBUG("检测到静态资源请求");
        FileHandler(request, response);
        //静态资源用 SendFile, 必须在当前IO线程直接发送
        WriteResponse(connection, request, *response);
        return;
    }

    //step2: 动态业务处理
    Handler handler;
    std::unordered_map<std::string, std::string> params;
    // 匹配路由
    if(MatchRoute(request._method, request._path, handler, params)) {
        SPDLOG_DEBUG("路由匹配成功，投递到业务线程池");
        //填充路径参数
        for(auto &kv : params) request.SetParam(kv.first, kv.second);
        // [关键]深拷贝 Request 对象
        // 因为 OnMessage 结束后 context 会被重置
        http::HttpRequest req_copy = request;
        // [关键]投递任务
        _worker_pool->Enqueue([connection, req_copy, handler, this](){
            // 这里是 worker 线程执行的
            //1. 创建全新的 Response 对象(栈上)
            http::HttpResponse async_resp(200);
            //2. 执行业务逻辑
            try { 
                handler(req_copy, &async_resp); 
            } catch (const std::exception &e) {
                SPDLOG_ERROR("业务异常: {}", e.what());
                async_resp._status = 500;
                async_resp.SetContent("Internal Server Error");
            } catch (...) {
                async_resp._status = 500;
                async_resp.SetContent("Unknown Error");
            }

            //3, 业务执行完，切换回 IO 线程发送
            connection->GetLoop()->RunInLoop([connection, req_copy, async_resp, this]() mutable {
                // 这里回到了 IO 线程
                this->WriteResponse(connection, req_copy, async_resp);
                if(async_resp.IsClose()) {
                    connection->Shutdown();
                }
            });
        });
    } else {
        SPDLOG_DEBUG("路由匹配失败");
        response->_status = 404;
        ErrorHandler(request, response);
        WriteResponse(connection, request, *response);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* brief: 向服务器注册可读事件触发后的处理函数 */
void HttpServer::OnMessage(const std::shared_ptr<src::Connection> &connection, src::Buffer *buffer) {
    while(buffer->ReadableBytes() > 0) {
        //step 1. 获取上下文数据
        http::HttpContext *context = std::any_cast<http::HttpContext>(connection->GetContext());
        //step 2. 通过上下文数据对缓冲区数据进行解析，得到HttpRequest对象
        // 1. 解析出错，直接进行错误响应
        // 2. 解析正常，且请求获取完毕，才开始去处理请求
        context->RecvHttpRequest(buffer);
        http::HttpRequest &request = context->GetRequest();
        SPDLOG_DEBUG("获取解析后的 HttpRequest 对象");
        http::HttpResponse response(context->GetRespStatus());
        SPDLOG_DEBUG("创建 HttpResponse, 并写入解析过程中产生的状态码");
        if(context->GetRespStatus() >= 400) {
            // 进行错误响应，关闭连接
            SPDLOG_DEBUG("状态码大于 400, 进行错误响应");
            ErrorHandler(request, &response); // 填充错误显示页面数据到response
            WriteResponse(connection, request, response); // 组织响应发送给客户端
            context->Reset();
            buffer->MoveReadOffset(buffer->ReadableBytes()); // 出错了就将缓冲区清空
            connection->Shutdown();
            return;
        }
        if(context->GetRecvStatus() != http::RECV_HTTP_OVER) {
            //当前请求还没有接收完毕，等待新数据到来继续处理
            SPDLOG_DEBUG("当前请求还未接收完整，等待新数据到来");
            return;
        }
        //step 3. 请求路由 + 业务处理
        SPDLOG_DEBUG("开始请求路由 + 业务处理");
        Dispatcher(connection, request, &response);
        //缩容buffer
        buffer->Shrink(src::Buffer::InitialSize);
        //重置上下文
        context->Reset();
    }
}

}