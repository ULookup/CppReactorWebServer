#pragma once

#include "../src/TcpServer.h"
#include "../util/Util.h"
#include "HttpContext.h"

namespace webserver::server
{

#define DEFAULT_TIMEOUT 30

class HttpServer
{
    using Handler = std::function<void(const http::HttpRequest&, http::HttpResponse*)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;
public:
    HttpServer(uint16_t port, int timeout = DEFAULT_TIMEOUT);
    /* brief: 提供给使用者注册基准路径 */
    void SetBaseDir(const std::string &path);
    /* brief: 提供给使用者注册GET方法业务函数 */
    void Get(const std::string &pattern, const Handler &handler) {
        _get_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    /* brief: 提供给使用者注册POST方法业务函数 */
    void Post(const std::string &pattern, const Handler &handler) {
        _post_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    /* brief: 提供给使用者注册PUT方法业务函数 */
    void Put(const std::string &pattern, const Handler &handler) {
        _put_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    /* brief: 提供给使用者注册DELETE方法业务函数 */
    void Delete(const std::string &pattern, const Handler &handler) {
        _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    /* brief: 提供给使用者来设置从属线程数 */
    void SetThreadCount(int count) { _server.SetThreadCount(count); }
    /* brief: 提供给使用者来启动服务器监听新连接的函数 */
    void Listen() { 
        //printf("进入Listen函数 启动服务器\n");
        _server.Start(); 
    }
private:
    /* brief: 开始响应 */
    void StartResponse(const std::shared_ptr<src::Connection> &connection) { connection->SetResponseStatus(false); }
    /* brief: 错误处理函数 */
    void ErrorHandler(const http::HttpRequest &request, http::HttpResponse *response);
    /* brief: 对应连接写入响应的函数 */
    void WriteResponse(const std::shared_ptr<src::Connection> &connection, const http::HttpRequest &request, http::HttpResponse &response);
    /* brief: 判断是不是静态资源请求 */
    bool IsFileHandler(const http::HttpRequest &request);
    /* brief: 静态资源处理函数 */
    void FileHandler(const http::HttpRequest &request, http::HttpResponse *response);
    /* brief: 对功能性请求进行路由分配的函数(已经确认了请求方法) */
    void Dispatcher(http::HttpRequest &request, http::HttpResponse *response, Handlers &handlers);
    /* brief: 对功能性请求进行路由(还没有确认方法) */
    void Route(http::HttpRequest &request, http::HttpResponse *response);
    /* brief: 向服务器注册连接成功后的处理函数 */
    void OnConnected(const std::shared_ptr<src::Connection> &connection) { connection->SetContext(http::HttpContext()); } // 设置该连接的协议上下文信息
    /* brief: 向服务器注册可读事件触发后的处理函数 */
    void OnMessage(const std::shared_ptr<src::Connection> &connection, src::Buffer *buffer);
private:
    Handlers _get_route;    // 保存使用者注册的GET方法的业务函数
    Handlers _post_route;   // 保存使用者注册的POST方法的业务函数
    Handlers _put_route;    // 保存使用者注册的PUT方法的业务函数
    Handlers _delete_route; // 保存使用者注册的DELETE方法的业务函数
    std::string _basedir;   // 保存使用者注册的基准路径
    src::TcpServer _server; // Tcp服务器
};

}