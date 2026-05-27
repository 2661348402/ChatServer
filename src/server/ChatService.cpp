#include "public.hpp"
#include "ChatService.hpp"
#include "Group.hpp"
#include <muduo/base/Logging.h>
#include <iostream>
#include <vector>
using namespace muduo;
using namespace std;

ChatService* ChatService::instance() {
    static ChatService service;
    return &service;
}

//登录
void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp) {
    LOG_INFO << "do login thing ";
    int id = js["id"]; 
    string pwd = js["password"]; 
    User user = _userModel.queryById(id);
    if(user.getId()==-1){
        json response;
        response["msgId"] = LOG_MSG_ACK;
        response["errno"] = 1;
        response["errMessage"] = "该用户不存在";
        conn->send(response.dump());
    }else if(user.getPwd() != pwd){
        json response;
        response["msgId"] = LOG_MSG_ACK;
        response["errno"] = 2;
        response["errMessage"] = "密码错误";
        conn->send(response.dump());
    }else{
        if(user.getState() == "online"){
            json response;
            response["msgId"] = LOG_MSG_ACK;
            response["errno"] = 3;
            response["errMessage"] = "请勿重复登录";
            conn->send(response.dump());
        }else{
            //成功登录
            {
                lock_guard<mutex> lock(connMutex);
               _userConnMap[user.getId()] = conn;
            }
            //redis订阅通道
            _redis.subscribe(id);

            //更新状态
            user.setState("online");
            if(!_userModel.updateState(user)){
                LOG_ERROR <<"更新状态失败";
            }
            json response;
            response["msgId"] = LOG_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            //获取离线消息
            vector<string> messages = _offlineMsgModel.query(user.getId());
            if(!messages.empty()){
                response["offlineMsg"] = messages;
                //删除离线消息
                _offlineMsgModel.remove(user.getId());
            }
            //返回群组列表
            vector<Group> groupVec = _groupModel.queryGroups(id);
            vector<string> groups;
            for(auto& group:groupVec){
                json js;
                js["name"] = group.getName();
                js["desc"] = group.getDesc();
                groups.push_back(js.dump());
            }
            response["groups"] = groups;

            //返回好友列表
            vector<string> friendVec;
            vector<User> userVec = _friendModel.query(id); 
            for(auto& user :userVec ){
                json js;
                js["id"] = user.getId();
                js["name"] = user.getName();
                js["state"] = user.getState();
                friendVec.push_back(js.dump());
            }
            response["friends"] = friendVec;
            conn->send(response.dump());

        }

    }

}
//注册
void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp) {
    LOG_INFO << "do reg thing ";
    string name = js["name"]; 
    string pwd = js["password"]; 
    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state) {
        //注册成功
        json response;
        response["msgId"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else {
        //注册失败
        json response;
        response["msgId"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());

    }

}
//点对点聊天
void ChatService:: oneChat(const TcpConnectionPtr& conn, json& js, Timestamp receiveTime){
    int toid = js["toid"];
    js["sendtime"] = receiveTime.toFormattedString(); 

    bool userState = false;
    {
        lock_guard<mutex> lock(connMutex);
        auto iter =_userConnMap.find(toid);
        if(iter !=_userConnMap.end()){
            iter->second->send(js.dump());
            return;
        }
    }
    //不在同一台服务器上
    User user = _userModel.queryById(toid);
    if(user.getState() == "online"){
        _redis.publish(toid,js.dump());
        return;
    }
    //存储离线消息
    _offlineMsgModel.insert(toid,js.dump());
}
//获取一个回调函数
MsgHandler ChatService::getHandler(int msgId) {
    auto iter = msgHandlerMap.find(msgId);
    if (iter == msgHandlerMap.end()) {
        return [=](const TcpConnectionPtr& conn, json& js, Timestamp) {
            LOG_ERROR << "msgId: " << msgId << "can't find the handler";
            };
    }
    else {
        return iter->second;
    }
}

ChatService::ChatService(){
    //注册回调函数
    msgHandlerMap.insert({LOGIN_MSG, bind(&ChatService::login,this,_1,_2,_3)});
    msgHandlerMap.insert({REG_MSG, bind(&ChatService::reg,this,_1,_2,_3)});
    msgHandlerMap.insert({ONE_CHAT_MSG, bind(&ChatService::oneChat,this,_1,_2,_3)});
    msgHandlerMap.insert({ADD_FRIEND_MSG, bind(&ChatService::addFriend,this,_1,_2,_3)});
    msgHandlerMap.insert({CREATE_GROUP_MSG, bind(&ChatService::createGroup,this,_1,_2,_3)});
    msgHandlerMap.insert({ADD_GROUP_MSG, bind(&ChatService::addGroup,this,_1,_2,_3)});
    msgHandlerMap.insert({GROUP_CHAT_MSG, bind(&ChatService::groupChat,this,_1,_2,_3)});
    msgHandlerMap.insert({LOGIN_OUT_MSG, bind(&ChatService::loginout,this,_1,_2,_3)});
    

    //连接redis服务器
    if(_redis.connect()){
        //设置上报消息回调函数
        _redis.init_notify_handler(bind(&ChatService::redisSubscribeMessage,this,_1,_2));
    }
}
//redis 订阅消息处理
void ChatService:: redisSubscribeMessage(int userid,string msg){
    //
    {
        lock_guard<mutex> lock(connMutex);
        auto iter = _userConnMap.find(userid);
        if(iter != _userConnMap.end()){
            iter->second->send(msg);
            return;
        }
    }
    //存储离线消息
    _offlineMsgModel.insert(userid,msg);
   
}
//登出
void ChatService:: loginout(const TcpConnectionPtr& conn, json& js, Timestamp receiveTime){
    int id = js["id"];
    {
        lock_guard<mutex> lock (connMutex);
        auto iter = _userConnMap.find(id);
        if(iter!=_userConnMap.end()){
               _userConnMap.erase(iter);
        }
    }
    //取消订阅
    _redis.unsubscribe(id);
    //更新离线状态
    User user;
    user.setId(id);
    user.setState("offline");
    _userModel.updateState(user);
    LOG_INFO <<"id: "<< id <<"已下线";

}
//处理客户端异常断开
void ChatService::clientConnectException(const TcpConnectionPtr& conn){
    int id = -1;
    {    
        lock_guard<mutex> lock (connMutex);
        for(auto iter = _userConnMap.begin();iter!=_userConnMap.end();iter++){
            if(iter->second == conn){
                id = iter->first;
               _userConnMap.erase(iter);
                break;
            }
        }
    }
    if(id != -1){
        //取消订阅
        _redis.unsubscribe(id);
        //更新离线状态
        User user;
        user.setId(id);
        user.setState("offline");
        _userModel.updateState(user);
        LOG_INFO <<"id: "<< id <<"已下线";
    }
}
//处理服务器异常
bool ChatService::reset(){
    return _userModel.stateReset();
}
//添加好友
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp){
    int friendId = js["friendId"];
    int userId = js["id"];
    _friendModel.insert(userId,friendId);
}


//创建群组
void  ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp){
    int userId = js["id"];
    string groupname = js["groupname"];
    string groupdesc = js["groupdesc"];
    Group group(-1,groupname,groupdesc);

    if(_groupModel.createGroup(group)){
        //加入创建者
        _groupModel.addGroup(userId,group.getId(),"creator");
    }
}
//加入群组
void  ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp){
    int userId = js["id"];
    int groupId = js["groupId"];
    _groupModel.addGroup(userId,groupId,"normal");
    
}
//群聊天
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp){
    int userId = js["id"];
    int groupId = js["groupId"];
    vector<int> userVec = _groupModel.queryGroupUsers(userId,groupId);

    {
        lock_guard<mutex> lock(connMutex);
        for(int& user_id :userVec ){
            auto iter =_userConnMap.find(user_id);
           
            if(iter !=_userConnMap.end()){
                 //在线消息
                iter->second->send(js.dump());
            }else{
                //不同服务器
                User user = _userModel.queryById(user_id);
                if(user.getState() == "online"){
                    _redis.publish(user_id,js.dump());
                }
                //离线消息
                _offlineMsgModel.insert(userId,js.dump());
            }
        }
    }

}

