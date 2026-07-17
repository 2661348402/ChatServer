


问题：
压测结束时大量连接同时断开，多个 IO 线程并发调用 Redis::unsubscribe()，
与 Redis 订阅线程共享同一个 hiredis redisContext，导致 hiredis context 并发读写，
最终触发 free(): invalid pointer。

原因：
hiredis redisContext 不是线程安全对象，不能被多个线程同时读写。

修复方向：
将 Redis publish 加互斥保护；
将 subscribe/unsubscribe 改为订阅线程内串行执行，业务线程通过线程安全队列提交订阅变更。



问题：
用户登录后，服务端日志显示 Redis 已连接：用户登录后，服务端日志显示 Redis 已连接：
但ChatServer 没有真正订阅用户 102 的 Redis 频道。

原因：
订阅连不上的核心原因，是改成队列模型后 redisGetReply 阻塞导致订阅命令无法及时执行；尝试用 redisSetTimeout 又导致 hiredis context 进入 EAGAIN 状态。正确做法是使用控制频道唤醒订阅线程，并确保 _subscribe_context 只由订阅线程串行访问。
