#include "../../include/json.hpp"
#include <iostream>
using namespace std;
using json = nlohmann::json;

//json 序列化示例
string func1() {
    json js;
    js["msg_type"] = 2;
    js["from"] = 2;
    js["to"] = "li si";
    js["msg"] = "hello ,what are you doing now?";
    string send_buff = js.dump();
    // cout << send_buff << endl;
    return send_buff;
}
string  func2() {
    json js;
    //添加数组
    js["id"] = { 1,2,3,4,5 };
    //添加key-value
    js["name"] = "zhang san";
    //添加对象
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    // 上面等同于下面这句一次性添加数组对象
    // js["msg"] = { {"zhang san", "hello world"},{"liu shuo", "hello china"} };
    cout << js << endl;
    return "";
}
string func3() {
    json js;
    //序列化一个vector容器
    vector<int> vec = { 1,2,3 };
    js["list"] = vec;
    //序列化一个map容器
    map<int, string> m = { {1,"黄山"},{2,"华山"},{3,"嵩山"} };
    js["path"] = m;
    string sendBuff = js.dump();
    cout << sendBuff << endl;
    return "";
}
void func4(){
    string jsonStr = R"({"id":1,"name":"zhang san","messages":"hello"})";
    json js = json::parse(jsonStr);
    cout << "id: " << js["id"]
         << " name: " <<js["name"]
         << " messages: "<<js["messages"]
         <<endl;
}
//反序列化
int main() {
    //序列化
    // func1();
    // func2();
    // func3();
    //反序列化
    func4();
    return 0;
}