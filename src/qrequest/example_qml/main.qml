import QtQuick 2.9
import QtQuick.Window 2.2

Window {
    visible: true
    width: 640
    height: 480
    title: "Hello World"

    Component.onCompleted: {
        request.get("http://127.0.0.1:8888/books", function(suc, msg, data){
            console.log("RESPONSE:", suc, msg, data);
        });
    }
}
