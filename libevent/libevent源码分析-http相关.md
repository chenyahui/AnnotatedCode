1. evhttp_new(ev_base_)
2. evhttp_set_gencb(ev_http_, handler_wrapper, this);
3. evhttp_bind_socket(ev_http_, ip, port) 
4. 
主要在第三步：
evhttp_bind_socket又调用了
evhttp_bind_socket_with_handle，其主要作用是三个：
1. 创建监听socket并监听该socket：
   bind_socket(0)
   listen()
2. 创建event、并绑定listener的callback。绑定的callback为 accept_socket_cb
    evhttp_accept_socket_with_handle
        1. evconnlistener_new
        2. evhttp_bind_listener

    在evconnlistener_new的时候，同时也会直接将该event添加到event_base中，是通过evconnlistener_enable中调用event_add实现的

3. event_base_dispatch(ev_base_); 开启事件循环


当accept事件到来的时候，会触发accept_socket_cb。
accept_socket_cb会做些什么事情呢?

accept_socket_cb的代码非常简单，实际上就是一行，直接调用了
evhttp_get_request函数，所以直接看evhttp_get_request
evhttp_get_request做了几件事：
1. evhttp_get_request_connection ： 新建连接
   1. bev = (*http->bevcb)(http->base, http->bevcbarg): 使用用户定义的callback创建一个bev(如果定义了callback的话)
   2. evhttp_connection_base_bufferevent_new: 1. 新建一个connection对象并且检查该连接的bufferevent之前有没有被bev_cb初始化, 没有的话，自己创建一个。 2. 设置了buffer_event的可读、可写、错误的callback分别为
   evhttp_read_cb, evhttp_write_cb, evhttp_error_cb
   3. 啥时候add到base里面呢？

2. TAILQ_INSERT_TAIL(&http->connections, evcon, next); 
   将新连接放入 server的连接队列中

3. evhttp_associate_new_request_with_connection
    1. 新建request对象: evhttp_request_new, 并设置其cb属性为evhttp_handle_request
    2. 关联这个request和conn
    3. 把request放到conn的requests队列中
    4. 重设置bufferevent的callback，并开始读conn的input buffer
在

buffer_event可读事件会触发evhttp_read_cb函数，即有新的request的时候就会触发这个函数,
这个函数是个典型的状态机的函数，把request的读取和解析分成了好几部分。每部分完成都会触发evcon->state的转化
成功状态下，最后一个状态机是evhttp_read_trailer

在该函数中，如果确认header读取完成，则调用evhttp_connection_done的函数。

evhttp_connection_done：

1. evcon->state = EVCON_WRITING;
2. (*req->cb)(req, req->cb_arg);evhttp_associate_new_request_with_connection中，我们将req的cb设置为evhttp_handle_request，所以实际上就是执行evhttp_handle_request

evhttp_handle_request就是正常的router过程

evhttp_send_reply : 回复
    evhttp_send
    evhttp_make_header写到buffer里面
    evhttp_write_buffer(evcon, evhttp_send_done, NULL): buffer写完后会调用evhttp_send_done
    evhttp_send_done:
    1. 调用req->on_complete_cb
	2. evhttp_request_free(req);
    3. 检测conn是否应该释放

    ?? 怎么把用户传的databuf和evcon的evbuffer连接起来的？？