#include "HttpRequest.h"

namespace webserver::http
{

void HttpRequest::Reset() {
    _method.clear();
    _path.clear();
    _version = "HTTP/1.1";
    _body.clear();
    _headers.clear();
    _params.clear();
}
/* brief: 判断是否存在指定头部字段 */
bool HttpRequest::HasHeader(const std::string &key) const {
    auto it = _headers.find(key);
    if(it == _headers.end()) return false;
    return true;
}
/* brief: 获取指定头部字段 */
std::string HttpRequest::GetHeader(const std::string &key) const {
    auto it = _headers.find(key);
    if(it == _headers.end()) return "";
    return it->second;
}
/* brief: 获取指定头部字段的视图（零拷贝） */
std::string_view HttpRequest::GetHeaderView(const std::string &key) const {
    auto it = _headers.find(key);
    if(it == _headers.end()) return {}; //返回空的 view
    return it->second; // 隐式转换为 string_view
}
/* brief: 判断是否存在指定查询字符串 */
bool HttpRequest::HasParam(const std::string &key) const {
    auto it = _params.find(key);
    if(it == _params.end()) return false;
    return true;
}
/* brief: 获取指定查询字符串 */
std::string HttpRequest::GetParam(const std::string &key) const {
    auto it = _params.find(key);
    if(it == _params.end()) return "";
    return it->second;
}
/* brief: 获取请求体大小 */
size_t HttpRequest::GetContentLength() const {
    bool ret = HasHeader("Content-Length");
    if(ret == false) {
        return 0;
    }
    std::string contlen = GetHeader("Content-Length");
    return std::stol(contlen);
}
/* brief: 判断是否是短连接 */
bool HttpRequest::IsClose() const {
    if(_version == "HTTP/1.0") {
        if(HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive") {
            return false;
        }
        return true;
    } else {
        if(HasHeader("Connection") == false || GetHeader("Connection") == "keep-alive") {
            return false;
        }
        return true;
    }
}

}