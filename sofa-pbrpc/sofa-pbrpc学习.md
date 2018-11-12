# 客户端请求server
以echo为例
1. Stub.Echo
2. channel.callMethod(echo_method_id, request, response, done)
3. 根据done == null来判断是否为sync
4. RpcClientImpl.callMethod(request, response, cntl)
   * 封装rpcmeta
5. RpcClientStream.call_method
6. RpcMessageStream.async_send_message()