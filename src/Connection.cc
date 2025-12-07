#include "Connection.h"

namespace webserver::src
{
Connection::Connection(EventLoop *loop, uint64_t conn_id, int sockfd)
    : _conn_id(conn_id), _sockfd(sockfd), _loop(loop), _enable_inactive_release(true),
    _status(CONNECTING), _socket(sockfd), _channel(_loop, _sockfd), _respctx()
    {
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
    }

/* brief: 建立函数，执行该函数即完成对一个连接的建立 */
void Connection::Established() { _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this)); }

/* brief: 发送数据，需要在对应的 EventLoop线程 内执行 */
void Connection::Send(const char *data, size_t len) {
    /* Buffer buf;
    buf.Append(data, len);
    _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buf))); */
    std::string str(data, len);
    _loop->RunInLoop([this, str = std::move(str)]() {
        if(_status == DISCONNECTED) return;
        _out_buffer.Append(str);
        if(!_channel.WritAble()) _channel.EnableWrite();
    });
}
/* brief: SendFile 发送 */
void Connection::SendFile(int fd, size_t size) {
    _loop->RunInLoop([this, fd, size]() {
        _respctx.sending = true;
        _respctx.header_sent = false;
        _respctx.expect_close = false;

        _respctx.fd = fd;
        _respctx.offset = 0;
        _respctx.remain = size;
        _respctx.active = true;

        // 如果当前没有可写事件，则开启写事件
        if(!_channel.WritAble()) _channel.EnableWrite();
    });
}
/* brief: 进入关闭连接流程，需要在对应的 EventLoop线程 内执行 */
void Connection::Shutdown() { _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this)); }
/* brief: 开启非活跃连接销毁，需要在对应的 EventLoop线程 内执行 */
void Connection::EnableInactiveRelease(int sec) { _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec)); }
/* brief: 关闭非活跃连接销毁，需要在对应的 EventLoop线程 内执行 */
void Connection::CancleInactiveRelease() { _loop->RunInLoop(std::bind(&Connection::CancleInactiveReleaseInLoop, this)); }
/* brief: 协议上下文切换函数，用于更新/切换连接使用的协议。需要在对应的 EventLoop线程 内执行 */
void Connection::Upgrade(const std::any &context,
                const ConnectedCallback &conncb,
                const MessageCallback &msgcb,
                const ClosedCallback &clscb,
                const AnyEventCallback &anyeventcb)
    {
        _loop->AssertInLoop();
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, conncb, msgcb, clscb, anyeventcb));
    }
/* brief: 开始响应 */
void Connection::StartResponse(bool close) { _loop->RunInLoop([this, close]{ StartResponseInLoop(close); }); }
/* brief: 结束响应头响应 */
void Connection::EndHeaderSend() {
    _respctx.header_sent = true;
}
/* brief: 判断是否还在发送响应 */
bool Connection::IsSendingResponse() const { return _respctx.sending; }
void Connection::SetResponseStatus(bool expect_close) { _respctx.expect_close = expect_close; }
// ===================================================================== //
// ============================== private ============================== //
// ===================================================================== //
/* brief：读事件就绪回调函数，用于 epoll 读事件就绪后，读取 socket输入缓冲区 的数据，并递交给上层使用者设置的业务函数处理 */
void Connection::HandleRead() {
    // epoll 监控的可读事件触发 EPOLLINT，channel 执行读事件回调
    // step1：读取 内核sockfd缓冲区 里的数据
    int savedError = 0;
    ssize_t ret = _in_buffer.ReadFd(_sockfd, &savedError);
    if(ret < 0) {
        // 读取失败，进入正常关闭连接流程：检查缓冲区还有没有待发送的数据
        return ShutdownInLoop();
    }
    SPDLOG_TRACE("[EventLoop: {}, Connection: {}] socket 缓冲区数据读取到 in_buffer内", _loop->GetId(), _conn_id);
    //step2：将读取到的数据交给上层进行业务处理
    if(_in_buffer.ReadableBytes() > 0) {
        // 将该连接的指针和缓冲区指针交付给上层，执行业务处理
        return _message_callback(shared_from_this(), &_in_buffer);
    }
}

/* brief：写事件回调函数，用于 epoll 写事件就绪后（即上层调用 Send 函数，把数据交给了连接的发送缓冲区），将连接发送缓冲区的内容传给 socket发送缓冲区 */
void Connection::HandleWrite() {
    // epoll 监控的可写事件触发 EPOLLOUT，channel 执行可写事件回调
    // step1：将发送缓冲区的数据传给 内核socket输出缓冲区
    if(_out_buffer.ReadableBytes() > 0) {
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 将输出缓冲区的数据传给内核 socket 发送缓冲区进行发送", _loop->GetId(), _conn_id);
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 输出缓冲区可读数据大小: {}", _loop->GetId(), _conn_id, _out_buffer.ReadableBytes());
        ssize_t ret = _socket.NonBlockSend(_out_buffer.ReadPos(), _out_buffer.ReadableBytes());
        if(ret < 0) {
            if(_in_buffer.ReadableBytes() > 0) {
                // 发送数据失败，如果接收缓冲区有数据，就先将还没处理的数据处理完
                _message_callback(shared_from_this(), &_in_buffer);
            }
            return Release(); //关闭连接
        }
        SPDLOG_TRACE("[EventLoop: {} Connection: {}]] 输出缓冲区发送了数据 size = {}", _loop->GetId(), _conn_id, ret);
        _out_buffer.MoveReadOffset(ret); //输出缓冲区发送了数据，将读偏移后移
    }
    // step2：判断发送缓冲区发送完没有且必须是请求头发送完了，发送完了才能发送静态资源
    // 注：这里是由Connection来管理发送的，不会出现一次请求没发送完就有下一次请求待发送的情况
    if(_out_buffer.ReadableBytes() == 0) {
        if (_status == DISCONNECTING) {
            SPDLOG_TRACE("SendFile 失败导致 DISCONNECTING, 由 HandleWrite 收尾关闭");
            return Release();
        }
        if(_respctx.active == true) {
            SPDLOG_TRACE("[EventLoop: {}, Connection: {}] Connection的静态资源发送上下文活跃中, 执行SendFileInLoop", _loop->GetId(), _conn_id);
            SendFileInLoop();
        }
        if(_respctx.active == true) {
            SPDLOG_TRACE("文件未发完，退出 HandleWrite 等待下一次 Loop");
            // 确保写事件是开启的
            if(_channel.WritAble() == false) _channel.EnableWrite();
            return; 
        }
        
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 响应完整发送结束，关闭可写事件", _loop->GetId(), _conn_id);
        _respctx.sending = false;
        _channel.DisableWrite();

        if(_respctx.expect_close || _status == DISCONNECTING) {
            SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 需要关闭连接", _loop->GetId(), _conn_id);
            return Release();
        }

        if(_in_buffer.ReadableBytes() > 0) {
            SPDLOG_TRACE("处理 Pipeline 滞留数据");
            _message_callback(shared_from_this(), &_in_buffer);
        }

        return;

    }

    SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 输出缓冲区还有数据待发送 size = {}, 退出HandleWrite, 等待下一次进入继续发送", _loop->GetId(), _conn_id, _out_buffer.ReadableBytes());
}

/* brief：关闭事件回调函数，用于 epoll 监测到 socket 连接断开后，同步关闭 Connection连接 */
void Connection::HandleClose() {
    if(_in_buffer.ReadableBytes() > 0) {
        // 如果输入缓冲区还有数据，就把没处理的数据处理了
        _message_callback(shared_from_this(), &_in_buffer);
    }
    return Release();
}

/* brief：错误事件回调函数，用于 epoll 监控事件出错，关闭 Connection 连接 */
void Connection::HandleError() { return HandleClose(); }

/* brief：任意事件回调函数，用于 epoll 监控到任意事件就绪，执行的函数 */
void Connection::HandleEvent() {
    //step1：刷新连接活跃度
    if(_enable_inactive_release) _loop->RefreshTimer(_conn_id);
    //step2：调用使用者设置的任意事件回调
    if(_anyevent_callback) _anyevent_callback(shared_from_this());
}

/* brief: 关闭并释放连接的函数，不能暴露给外部，需要在对应的 EventLoop线程 内执行 */
void Connection::Release() {
    _loop->PushInLoop(std::bind(&Connection::ReleaseInLoop, this));
}

/* brief：建立连接的函数，将连接状态置为已连接，然后开始对读事件的监控，并调用使用者设置的建立连接回调函数 */
void Connection::EstablishedInLoop() {
    //step1：修改连接状态
    assert(_status == CONNECTING);
    _status = CONNECTED;
    //一旦启动了读事件监控就有可能立即触发读事件，这时候如果启动了非活跃连接销毁就会出错
    //step2：启动读事件监控
    _channel.EnableRead();
    //step3：调用回调函数
    if(_connected_callback) _connected_callback(shared_from_this());
}

/* brief：释放/断开 Connection 连接的函数，将连接状态置为已关闭，然后移除对事件的监控，再关闭文件描述符，取消定时任务，调用上层的关闭连接回调函数 */
void Connection::ReleaseInLoop() {
    if(_status == DISCONNECTED) return;
    //step1：修改连接状态，置为DISCONNECTED
    _status = DISCONNECTED;
    //step2：移除连接的事件监控
    _channel.Remove();
    //step3：关闭描述符
    _socket.Close();
    // === FIX START: 关闭残留的文件描述符 ===
    if (_respctx.active && _respctx.fd >= 0) {
        close(_respctx.fd);
        _respctx.fd = -1;
        _respctx.active = false;
        SPDLOG_INFO("连接关闭，清理未发送完的文件 fd");
    }
    //step4：如果有定时销毁任务，就取消任务
    if(_loop->HasTimer(_conn_id)) CancleInactiveReleaseInLoop();
    //step5：调用关闭回调函数（避免先移除服务器的连接管理信息导致Connection释放后的处理（use-after-free）
    if(_closed_callback) _closed_callback(shared_from_this());
    if(_server_closed_callback) _server_closed_callback(shared_from_this());
}

/* brief：连接的发送数据函数，将要发送的数据交给 Connection发送缓冲区，然后开启读事件监控 */
void Connection::SendInLoop(Buffer &buf) {
    if(_status == DISCONNECTED) return;
    //将要发送的数据放入连接的发送缓冲区，并开启可写事件监控，表示可以写入内核缓冲区了
    SPDLOG_TRACE("将要发送的数据放入连接的输出缓冲区");
    _out_buffer.Append(buf);
    SPDLOG_TRACE("输出缓冲区可读字节数: {}", _out_buffer.ReadableBytes());
    if(_channel.WritAble() == false) _channel.EnableWrite();
}
/* brief: 实际发送的函数 */
void Connection::SendFileInLoop() {
    //step 1: 检测静态资源上下文是否活跃（是否需要发送）
    if(!_respctx.active) return;   
    //step 2: 发送静态资源
    while(_respctx.remain > 0) {
        size_t send_size = std::min(_respctx.remain, (size_t)0x7ffff000);
        ssize_t sent = sendfile(_sockfd, _respctx.fd, (off_t *)&_respctx.offset, send_size);
        SPDLOG_TRACE("[Connection: {}, Channel: {}] SendFile发送数据", _conn_id, _channel.GetFd());
        if(sent > 0) {
            SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 发送前remain = {}, 发送了sent = {}", _loop->GetId(), _conn_id, _respctx.remain, sent);
            _respctx.remain -= sent;
            // 注意：sendfile 如果传入 offset 指针，它会自动更新 offset 对应的值吗？
            // Linux sendfile man page: "If offset is not NULL, then it points to a variable holding the file offset... 
            // When sendfile() returns, this variable will be set to the offset of the byte following the last byte that was read."
            // 所以不需要手动 _respctx.offset += sent; (前提是传递的是指针)
            // 代码：(off_t *)&_respctx.offset 是对的。

            if (_respctx.remain == 0) {
            // 文件发送真正完成
            SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 静态资源发送完毕", _loop->GetId(), _conn_id);
                close(_respctx.fd);
                _respctx.fd = -1;
                _respctx.active = false;
                break;
            }
            SPDLOG_TRACE("发送了数据，但还没发送完，循环尝试发送");
        } else {
            if(errno == EAGAIN || errno == EINTR) {
                // 发送缓冲区满了，开启写事件监控，等待下一次发送
                SPDLOG_TRACE("[EventLoop: {}, Connection: {}] sendfile 发送失败, 开启写事件监控, 等待下一次发送", _loop->GetId(), _conn_id);
                if(_channel.WritAble() == false) _channel.EnableWrite();
                break;
            } else {
                // 发送错误，需要关闭连接
                SPDLOG_ERROR("[EventLoop: {}, Connection: {}] sendfile 发送错误, errno = {}", _loop->GetId(), _conn_id, errno);
                close(_respctx.fd);
                _respctx.fd = -1;
                _respctx.active = false;

                // 由 HandleWrite 触发 Release
                _status = DISCONNECTING;
                break;
            }
        }
    }
}

/* brief：关闭连接的函数，执行实际断开/销毁连接前的流程，再调用实际的断开/销毁函数 */
void Connection::ShutdownInLoop() {
    _status = DISCONNECTING;
    //如果接收缓冲区还有数据，就先把数据处理了
    if(_in_buffer.ReadableBytes() > 0) {
        if(_message_callback) _message_callback(shared_from_this(), &_in_buffer);
    }
    //如果发送缓冲区还有数据，就把数据发送了
    if(_out_buffer.ReadableBytes() > 0) {
        //开启可写事件监控，表示可以写入内核缓冲区了
        if(_channel.WritAble() == false) _channel.EnableWrite();
    }
    //如果静态资源上下文还活跃，就把静态资源发送了
    if(_respctx.active) {
        if(_channel.WritAble() == false) _channel.EnableWrite();
    }
    if(_out_buffer.ReadableBytes() == 0 && _respctx.active == false) Release();
}

/* brief：开启超时连接销毁机制，并设定时间 */
void Connection::EnableInactiveReleaseInLoop(int sec) {
    //step1：将判断标识置为true
    _enable_inactive_release = true;
    //step2：添加/刷新定时销毁任务
    if(_loop->HasTimer(_conn_id)) return _loop->RefreshTimer(_conn_id);
    _loop->AddTimer(_conn_id, sec, std::bind(&Connection::Release, this));
}

/* brief：关闭超时连接销毁机制 */
void Connection::CancleInactiveReleaseInLoop() {
    _enable_inactive_release = false;
    if(_loop->HasTimer(_conn_id)) return _loop->CancelTimer(_conn_id);
}

/* brief：更新协议上下文 */
void Connection::UpgradeInLoop(const std::any &context,
                const ConnectedCallback &conncb,
                const MessageCallback &msgcb,
                const ClosedCallback &clscb,
                const AnyEventCallback &anyeventcb) 
    {
        _context = context;
        _connected_callback = conncb;
        _message_callback = msgcb;
        _closed_callback = clscb;
        _anyevent_callback = anyeventcb;
    }

/* brief: 开始响应 */
void Connection::StartResponseInLoop(bool expect_close) {
    // 开始一次新的响应
    _respctx.sending = true;
    _respctx.header_sent = false;
    _respctx.expect_close = expect_close;

    // 清理 body/sendfile 状态（以防上次残留）
    _respctx.fd = -1;
    _respctx.offset = 0;
    _respctx.remain = 0;
    _respctx.active = false;
}
void Connection::EndHeaderSendInLoop() {
    // 标记已发送完 header
    _respctx.header_sent = true;
    SPDLOG_TRACE("[EventLoop:{}, Conn:{}] 已经将响应头的发送压入任务队列", _loop->GetId(), _conn_id);
}

}