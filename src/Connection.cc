#include "Connection.h"

namespace webserver::src
{
Connection::Connection(EventLoop *loop, uint64_t conn_id, int sockfd)
    : _conn_id(conn_id), _sockfd(sockfd), _loop(loop), _enable_inactive_release(true),
    _status(CONNECTING), _socket(sockfd), _channel(_loop, _sockfd), _filectx()
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
void Connection::SendFile(int fd, off_t offset, size_t size) {
    _loop->RunInLoop(std::bind(&Connection::SendFileInLoop, this, fd, offset, size));
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
    // step1: 优先发送内存Buffer（通常Headers在这里）
    if(_out_buffer.ReadableBytes() > 0) {
        // 内存输出缓冲区Buffer有数据，发送
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 输出缓冲区有数据待发送: {}", _loop->GetId(), _conn_id, _out_buffer.ReadableBytes());
        ssize_t ret = _socket.NonBlockSend(_out_buffer.ReadPos(), _out_buffer.ReadableBytes());
        if(ret < 0) {
            // Socket发送数据失败（一般是对端关闭连接)
            return Release();
        }
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 输出缓冲区发送了数据: {}", _loop->GetId(), _conn_id, ret);
        _out_buffer.MoveReadOffset(ret);
    }

    // step2: 如果内存输出缓冲区Buffer发送完了，检查是否有文件要发送（Body通常在这里）
    if(_out_buffer.ReadableBytes() == 0 && _filectx.active) {
        // 原来SendFileInLoop 的逻辑移到这里
        // 真正的 sendfile 系统调用逻辑
        size_t send_len = std::min(_filectx.remain, kMaxSendChunk);
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 需要发送文件的大小为: {}bytes", _loop->GetId(), _conn_id, send_len);
        ssize_t sent = sendfile(_sockfd, _filectx.fd, &_filectx.offset, send_len);
        if(sent > 0) {
            SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 发送了 {}bytes 的文件", _loop->GetId(), _conn_id, sent);
            _filectx.remain -= sent;
            if(_filectx.remain == 0) {
                SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 文件发送完毕", _loop->GetId(), _conn_id);
                close(_filectx.fd);
                _filectx.Reset();
            }
        } else {
            if(errno != EAGAIN || errno != EINTR) {
                SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 文件发送失败, 关闭并释放连接");
                close(_filectx.fd);
                _filectx.Reset();
                return Release();
            }
        }
    }

    // step3: 检查内存输出缓冲区Buffer和文件是否发送完毕
    if(_out_buffer.ReadableBytes() == 0 && !_filectx.active) {
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 输出缓冲区和文件没有待发送的数据", _loop->GetId(), _conn_id);
        _channel.DisableWrite();

        if(_status == DISCONNECTING) {
            // 如果处于DISCONNECTING状态，说明上层已经调用过 Shutdown()，现在数据发完了，可以真正关闭了
            Release();
        }
    }
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
    if (_filectx.active && _filectx.fd >= 0) {
        close(_filectx.fd);
        _filectx.Reset();
        SPDLOG_INFO("连接关闭，清理文件");
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
void Connection::SendFileInLoop(int fd, off_t offset, size_t size) {
    // 如果上一个文件还没发送完，这里需要处理策略（报错或排队）
    if(_filectx.active) {
        //上一个文件还没发完，应用层回等待上一条发完
        close(fd);
        SPDLOG_WARN("Connection繁忙, 不发送该文件");
        return;       
    }
    // 上一个文件发完了, Connection 空闲
    _filectx.fd = fd;
    _filectx.offset = offset;
    _filectx.remain = size;
    _filectx.active = true;

    if(!_channel.WritAble()) _channel.EnableWrite();
}

/* brief：关闭连接的函数，执行实际断开/销毁连接前的流程，再调用实际的断开/销毁函数 */
void Connection::ShutdownInLoop() {
    _status = DISCONNECTING;
    //如果接收缓冲区还有数据，就先把数据处理了
    if(_in_buffer.ReadableBytes() > 0) {
        if(_message_callback) _message_callback(shared_from_this(), &_in_buffer);
    }
    //只有当所有数据（Buffer 和 File）都发完了，才直接Release
    if(_out_buffer.ReadableBytes() == 0 && !_filectx.active) {
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 所以数据都发送完了，直接释放连接", _loop->GetId(), _conn_id);
        Release();
    } else {
        //还有数据没发完，确保 Write 事件开启，让 HandleWrite在发完后触发 Release
        SPDLOG_TRACE("[EventLoop: {}, Connection: {}] 还有数据没发完，开启可写事件监控", _loop->GetId(), _conn_id);
        if(!_channel.WritAble()) _channel.EnableWrite();
    }
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
    // 清理 body/sendfile 状态（以防上次残留）
    _filectx.Reset();
}

}