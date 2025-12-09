#include "Util.h"

namespace webserver::util
{
std::unordered_map<int, std::string> status_message = {
    {100, "Continue"},
    {101, "Switching Protocol"},
    {102, "Processing"},
    {103, "Early Hints"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choice"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "unused"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Payload Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {425, "Too Early"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {451, "Unavailable For Legal Reasons"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},          
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"}
};

std::unordered_map<std::string, std::string> mime_message = {
    {".aac", "audio/aac"},
    {".abw", "application/x-abiword"},
    {".arc", "application/x-freearc"},
    {".avi", "video/x-msvideo"},
    {".azw", "application/vnd.amazon.ebook"},
    {".bin", "application/octet-stream"},
    {".bmp", "image/bmp"},
    {".bz", "application/x-bzip"},
    {".bz2", "application/x-bzip2"},
    {".csh", "application/x-csh"},
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformatsofficedocument.wordprocessingml.document"},
    {".eot", "application/vnd.ms-fontobject"},
    {".epub", "application/epub+zip"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".ico", "image/vnd.microsoft.icon"},
    {".ics", "text/calendar"},
    {".jar", "application/java-archive"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".jsonld", "application/ld+json"},
    {".mid", "audio/midi"},
    {".midi", "audio/x-midi"},
    {".mjs", "text/javascript"},
    {".mp3", "audio/mpeg"},
    {".mpeg", "video/mpeg"},
    {".mpkg", "application/vnd.apple.installer+xml"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".oga", "audio/ogg"},
    {".ogv", "video/ogg"},
    {".ogx", "application/ogg"},
    {".otf", "font/otf"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformatsofficedocument.presentationml.presentation"},
    {".rar", "application/x-rar-compressed"},
    {".rtf", "application/rtf"},
    {".sh", "application/x-sh"},
    {".svg", "image/svg+xml"},
    {".swf", "application/x-shockwave-flash"},
    {".tar", "application/x-tar"},
    {".tif", "image/tiff"},
    {".tiff", "image/tiff"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".vsd", "application/vnd.visio"},
    {".wav", "audio/wav"},
    {".weba", "audio/webm"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformatsofficedocument.spreadsheetml.sheet"},
    {".xml", "application/xml"},
    {".xul", "application/vnd.mozilla.xul+xml"},
    {".zip", "application/zip"},
    {".3gp", "video/3gpp"},
    {".3g2", "video/3gpp2"},
    {".7z", "application/x-7z-compressed"}
};   


/* brief: 分割字符串 */
size_t Util::Split(const std::string_view &src, const std::string &sep, std::vector<std::string_view> *arry) {
    size_t offset = 0; // 游标
    while(offset < src.size()) {
        size_t pos = src.find(sep, offset); // 从游标开始找sep
        if(pos == std::string::npos) {
            // 没找到分割符sep
            if(pos == src.size()) break;
            arry->push_back(src.substr(offset));
            return arry->size();
        }
        if(pos == offset) {
            // offset 自身位置就是分割符
            offset = pos + sep.size();
            continue;
        }
        arry->push_back(src.substr(offset, pos - offset));
        offset = pos + sep.size();
    }

    return arry->size();
}
/* brief: 16进制转10进制 */
char Util::HexToInt(char c) {
    if(c >= '0' && c <= '9') {
            return c - '0';
        }else if(c >= 'a' && c <= 'z') {
            return c - 'a' + 10;
        }else if(c >= 'A' && c <= 'Z') {
            return c - 'A' + 10;
        }
    return -1;
}
/* brief: 分割Http请求行 */
std::vector<std::string_view> Util::SplitLine(const std::string &line) {
    std::string_view line_view = line;
    std::vector<std::string_view> result;
    size_t current_global_pos = 0; // 在line 上跟踪 Method结束的位置
    //step1: 查找第一个空格
    size_t sep = line_view.find(' ');

    //处理只有方法或格式错误的情况
    if(sep == std::string_view::npos) {
        // 找不到空格，则将整行视为方法（这是错误情况）
        result.emplace_back(line_view); //请求方法
        result.emplace_back("");   //请求路径
        result.emplace_back("");   //请求参数
        result.emplace_back("");   //协议版本
        return result;
    }

    //找到了空格，放入 Method
    result.emplace_back(line_view.substr(0, sep)); // 请求方法
    SPDLOG_TRACE("Method(line_view.substr(0, sep)): {}", line_view.substr(0, sep));
    current_global_pos = sep + 1; // 游标移动到空格后的位置（请求url开始）

    //step2: 从游标开始查找第二个空格
    sep = line_view.find(' ', current_global_pos);

    std::string_view url;
    if(sep == std::string_view::npos) {
        // 没有找到空格（即可能缺少协议版本）
        url = line_view.substr(current_global_pos);
    } else {
        // 找到了空格
        url = line_view.substr(current_global_pos, sep - current_global_pos);
        SPDLOG_TRACE("Url(line_view.substr(current_global_pos, sep - current_global_pos)): {}", url);   
    }

    //step3: 在URI内部查找 '?' ，分割 Path 和 Params
    size_t url_end = sep; // 保持uri的结尾空格位置
    sep = url.find('?'); // 从 uri的局部索引 0 开始查找
    if(sep == std::string_view::npos) {
        // 没有参数
        result.emplace_back(url); // 请求路径
        SPDLOG_TRACE("Path(Url): {}", url);   
        result.emplace_back("");  // 空请求参数
        SPDLOG_TRACE("Params(NULL): NULL");
    } else {
        // 有参数
        result.emplace_back(url.substr(0, sep)); // 放入Path，局部索引从0到Sep
        SPDLOG_TRACE("Path(uri.substr(0, sep)): {}", url.substr(0, sep));  
        result.emplace_back(url.substr(sep + 1)); // 放入Params，局部索引从Sep + 1到结尾
        SPDLOG_TRACE("Params(uri.substr(sep + 1)): {}", url.substr(sep + 1));   
    }
    
    //step4: 放入Version
    if(url_end != std::string_view::npos) {
        result.emplace_back(line_view.substr(url_end + 1)); // 协议版本
        SPDLOG_TRACE("Version(line_view.substr(uri_end + 1)): {}", line_view.substr(url_end + 1));   
    } else {
        result.emplace_back(""); // 缺少协议版本
        SPDLOG_TRACE("Version(NULL): NULL");   
    }

    return result;
}
/* brief: 对Url进行解码 */
std::string Util::UrlDecode(const std::string_view &url, bool is_convert_space_to_plus) {
    //遇到了 % 就将后面两个字符转化为数字，第一位数字左移4位，然后加上第二位数字 eg: + -> 2b %2b -> 2 << 4 + 11
    std::string res;
    for(int i = 0; i < url.size(); ++i) {
        if(url[i] == '+' && is_convert_space_to_plus == true) {
            res += ' ';
            continue;
        }
        if(url[i] == '%' && (i + 2) < url.size()) {
            char v1 = HexToInt(url[i + 1]);
            char v2 = HexToInt(url[i + 2]);
            char v = (v1 << 4) + v2;
            res += v;
            i += 2;
            continue;
        }
        res += url[i];
    }
    return res;
}
/* brief: 判断路径是否是一个目录 */
bool Util::IsDirectory(const std::string &filename) {
    struct stat st;
    int ret = stat(filename.c_str(), &st);
    if(ret < 0) return false;
    return S_ISDIR(st.st_mode);
}
/* brief: 判断路径是否是一个普通文件 */
bool Util::IsRegular(const std::string &filename) {
    struct stat st;
    int ret = stat(filename.c_str(), &st);
    if(ret < 0) return false;
    return S_ISREG(st.st_mode);
}
/* brief: 读取文件(需要强复习) */
bool Util::ReadFile(const std::string &filename, std::string *buf) {
    std::ifstream ifs(filename, std::ios::binary);
    if(ifs.is_open() == false) return false;
    size_t fsize = 0;
    ifs.seekg(0, ifs.end);
    fsize = ifs.tellg();
    ifs.seekg(0, ifs.beg); //跳转到起始位置

    buf->resize(fsize);
    ifs.read(&(*buf)[0], fsize);
    if(ifs.good() == false) {
        ifs.close();
        return false;
    }
    ifs.close();
    return true;
}
/* brief: 写入文件 */
bool Util::WriteFile(const std::string &filename, const std::string &buf) {
    std::ofstream ofs(filename, std::ios::binary);
    if(ofs.is_open() == false) return false;
    ofs.write(buf.c_str(), buf.size());
    if(ofs.good() == false) {
        ofs.close();
        return false;
    }
    ofs.close();
    return true;
}
/* brief: 响应状态码的描述信息获取 */
std::string Util::StatusDesc(int status) {
    auto it = status_message.find(status);
    if(it != status_message.end()) {
        return it->second;
    }
    return "Unknow";
}
/* brief: 判断请求路径是否有效 */
bool Util::ValidPath(const std::string &path) {
    //思想：按照 / 进行路径分割，根据有多少子目录，确定有多少层，深度不小于0
    std::vector<std::string_view> subdir;
    Split(path, "/", &subdir);
    int level = 0;
    for(auto &dir : subdir) {
        if(dir == "..") {
            level --;
            if(level < 0) return false;
            continue;
        }
        if(dir == ".") continue;
        level++;
    }
    return true;
}
/* brief: 根据文件后缀名获取文件mime */
std::string Util::ExtMime(const std::string &filename) {
    size_t pos = filename.find_last_of('.');
    if(pos == std::string::npos) {
        return "application/octet-stream";
    }
    std::string ext = filename.substr(pos);
    auto it = mime_message.find(ext);
    if(it == mime_message.end()) {
        return "application/octet-stream";
    }
    return it->second;
}
/* brief: 分割请求路径 */
std::vector<std::string> Util::SplitPath(const std::string &path) {
    std::vector<std::string> result;
    if(path.empty()) return result;

    size_t start = 0;
    //跳过开头的 '/'
    if(path[0] == '/') start = 1;

    size_t end = path.find('/', start);
    while(end != std::string::npos) {
        if(end > start) {
            SPDLOG_TRACE("解析到一个结点: {}", path.substr(start, end - start));
            result.emplace_back(path.substr(start, end - start));
        }
        start = end + 1;
        end = path.find('/', start);
    }
    if(start < path.length()) {
        SPDLOG_TRACE("解析到一个结点: {}", path.substr(start));
        result.emplace_back(path.substr(start));
    }

    return result;
}
/* brief: 解析Range请求 */
bool Util::ParseRange(std::string_view range, size_t file_size, off_t &start, off_t &end) {
    // Range 格式通常为: "bytes=0-499" 或 "bytes=500-"
    // 1. 默认预设值
    start = 0;
    end = file_size - 1;

    // 2. 移除 "bytes=" 前缀
    if(range.size() < 6 || range.substr(0, 6) != "bytes=");

    range.remove_prefix(6);

    // 3. 查找分割符 '-'
    size_t split = range.find('-');
    if(split == std::string_view::npos) return false;

    // 4. 切割出 start 和 end 的视图
    std::string_view s_start = range.substr(0, split);
    std::string_view s_end = range.substr(split + 1);

    // 5. 解析 Start (利用 std::from_chars)
    if(!s_start.empty()) {
        // 去除可能的空格
        while(!s_start.empty() && s_start.front() == ' ') s_start.remove_prefix(1);

        auto res = std::from_chars(s_start.data(), s_start.data() + s_start.size(), start);
        if(res.ec != std::errc()) return false;
        // 处理 end
        if(!s_end.empty()) {
            while(!s_end.empty() && s_end.front() == ' ') s_end.remove_prefix(1);
            
            //正常范: bytes=0-499
            off_t temp_end = 0;
            auto res_end = std::from_chars(s_end.data(), s_end.data() + s_end.size(), temp_end);
            if(res_end.ec != std::errc()) return false;

            if(temp_end < static_cast<off_t>(file_size)) {
                end = temp_end;
            }
        }
    } else {
        // 处理 bytes=-500 的情况
        if(!s_end.empty()) {
            while(!s_end.empty() && s_end.front() == ' ') s_end.remove_prefix(1);

            off_t suffix = 0;
            auto res = std::from_chars(s_end.data(), s_end.data() + s_end.size(), suffix);
            if(res.ec != std::errc()) return false;

            start = file_size - suffix;
            if(start < 0) start = 0;
        } else {
            return false;
        }
    }
    return true;
}

}