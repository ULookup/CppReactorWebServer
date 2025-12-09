#pragma once

#include "../src/Buffer.h"
#include <unordered_map>
#include <string_view>
#include <iostream>
#include <sstream>
#include <fstream>
#include <charconv>
#include <sys/stat.h>
#include <spdlog/spdlog.h>

namespace webserver::util
{

class Util
{
public:
    /* brief： 16进制转10进制 */
    static char HexToInt(char c);
    /* brief:  字符串分割 */
    static size_t Split(const std::string_view &src, const std::string &sep, std::vector<std::string_view> *arry);
    /* brief: 将请求行分割为请求方法/请求资源路径/请求参数/协议版本 */
    static std::vector<std::string_view> SplitLine(const std::string &line);
    /* brief: 对uri进行解码 */
    static std::string UrlDecode(const std::string_view &url, bool is_convert_space_to_plus);
    /* brief: 判断路径是否是一个目录 */
    static bool IsDirectory(const std::string &filename);
    /* brief: 判断路径是否是一个普通文件 */
    static bool IsRegular(const std::string &filename);
    /* brief: 读取文件 */
    static bool ReadFile(const std::string &filename, std::string *buf);
    /* brief: 写入文件 */
    static bool WriteFile(const std::string &filename, const std::string &buf);
    /* brief: 响应状态码的描述信息获取 */
    static std::string StatusDesc(int status);
    /* brief: 判断请求路径是否有效 */
    static bool ValidPath(const std::string &path);
    /* brief: 根据文件后缀名获取文件mime */
    static std::string ExtMime(const std::string &filename);
    /* brief: 分割请求路径 */
    static std::vector<std::string> SplitPath(const std::string &path);
    /* brief: 解析Range请求 */
    static bool ParseRange(std::string_view range, size_t file_size, off_t &start, off_t &end);
};



}