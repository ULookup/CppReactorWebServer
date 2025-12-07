#include "HttpServer.h"
#include <spdlog/spdlog.h>

namespace webserver::server
{
/* brief: 创建服务器，并将消息处理函数绑定到server里 */
HttpServer::HttpServer(uint16_t port, int timeout) : _server(port) {
    _server.EnableInactiveRelease(timeout);
    _server.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1)); // 将OnConnection的this绑定后，剩下的Connection参数交给服务器去绑定
    _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
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
    // 3.1 如果是常规响应，直接发送 body
    if(!response.HasHeader("X-SENDFILE-FD")) {
        SPDLOG_TRACE("请求属于非静态资源请求, 使用SendFile实现零拷贝");
        connection->Send(header.c_str(), header.size());
        if(!response._body.empty()) {
            SPDLOG_TRACE("HttpResponse 的 Body:");
            SPDLOG_TRACE("{}", response._body);
            connection->Send(response._body.c_str(), response._body.size());
        }
        return;
    }
    // 3.2 是SendFile响应
    SPDLOG_TRACE("请求属于静态资源请求, 使用SendFile实现零拷贝");
    int fd = std::stoi(response._headers["X-SENDFILE-FD"]);
    size_t len = std::stoul(response._headers["X-SENDFILE-SIZE"]);
    SPDLOG_TRACE("调用Send, 发送响应头");
    connection->Send(header.c_str(), header.size());
    connection->EndHeaderSend();
    
    SPDLOG_TRACE("调用SendFile, 静态资源上下文交给Connection管理");
    connection->SendFile(fd, len);
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
    int fd = open(request_path.c_str(), O_RDONLY);
    if(fd < 0) {
        response->_status = 404;
        ErrorHandler(request, response);
        return;
    }
    struct stat st;
    if(fstat(fd, &st) < 0) {
        close(fd);
        response->_status = 404;
        ErrorHandler(request, response);
        return;
    }

    response->_status = 200;
    response->SetHeader("Content-Length", std::to_string(st.st_size));
    response->SetHeader("Content-Type", util::Util::ExtMime(request_path));

    response->_body.clear();
    response->_headers["X-SENDFILE-FD"] = std::to_string(fd);
    response->_headers["X-SENDFILE-SIZE"] = std::to_string(st.st_size);
    // 判断 MIME 是否以 image/ 开头，如果是，就开启缓存
    /* if(mime.rfind("image/", 0) == 0) response->SetHeader("Cache-Control", "max-age=31536000, public"); */
    SPDLOG_DEBUG("退出FileHandler函数");
}
/* brief: 对功能性请求进行路由分配的函数(已经确认了请求方法) */
void HttpServer::Dispatcher(http::HttpRequest &request, http::HttpResponse *response, Handlers &handlers) {
    //在对应请求方法的路由表中，查找是否含有对应处理函数，有则调用，没有则返回404
    //思想：路由表存储的是键值对 --- 正则表达式 & 处理函数
    //使用正则表达式，对请求的资源路径进行正则匹配，匹配成功就使用对应函数处理
    //（需要考虑怎么优化掉正则匹配）
    SPDLOG_DEBUG("对功能性请求进行分类处理");
    for(auto &handler : handlers) {
        const std::regex &re(handler.first);
        const Handler &functor = handler.second;
        bool ret = std::regex_match(request._path, request._matches, re);
        if(ret == false) continue;

        SPDLOG_DEBUG("找到请求的函数方法");
        return functor(request, response); // 执行功能性请求函数，传入空response和请求信息
    }
    SPDLOG_WARN("没有找到请求的函数方法");
    response->_status = 404;
}
 /* brief: 对功能性请求进行路由(还没有确认方法) */
void HttpServer::Route(http::HttpRequest &request, http::HttpResponse *response) {
    //step 1:对请求进行分辨，是一个静态资源请求，还是一个功能性请求
    // 静态资源请求，则进行静态资源的处理
    // 功能性请求，则需要通过几个请求路由表来确定是否有处理函数
    // GET\HEAD 都先默认是静态资源请求
    if(IsFileHandler(request) == true) {
        return FileHandler(request, response);
    }
    SPDLOG_DEBUG("开始对非静态资源请求进行路由");
    if(request._method == "GET" || request._method == "") {
        SPDLOG_TRACE("方法为 GET");
        return Dispatcher(request, response, _get_route);
    }
    else if(request._method == "POST") {
        SPDLOG_TRACE("方法为 POST");
        return Dispatcher(request, response, _post_route);
    }
    else if(request._method == "PUT") {
        SPDLOG_TRACE("方法为 PUT");
        return Dispatcher(request, response, _put_route);
    }
    else if(request._method == "DELETE") {
        SPDLOG_TRACE("方法为 DELETE");
        return Dispatcher(request, response, _delete_route);
    }

    response->_status = 405; // METHOD NOT ALLOWED
    return;
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
        Route(request, &response);
        //step 4. 对HttpResponse进行组织发送
        WriteResponse(connection, request, response);
        //缩容buffer
        buffer->Shrink(src::Buffer::InitialSize);
        //重置上下文
        context->Reset();
        //step 5. 根据长短连接判断是否关闭连接或者继续处理
        if(response.IsClose()) { connection->Shutdown(); }
    }
}

}