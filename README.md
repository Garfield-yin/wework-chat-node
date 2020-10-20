# wework-chat-node
使用 node-addon-api 封装了企业微信会话存档金融版 SDK 接口，提供给 node.js 直接调用。

[企业微信获取会话内容文档链接]https://work.weixin.qq.com/api/doc/90000/90135/91774

### Installation
```
npm install wework-chat-node
```
如果需要升级企业微信SDK,请更新 lib/libWeWorkFinanceSdk_C.so 以及 include/wework/WeWorkFinanceSdk_C.h，文件更新后再build。
本模块也会持续更新优化。

##### Compiling
由于企业微信提供的 sdk 仅支持 linux 与 windows,在 OS X 下可编译成功，但无法正常使用。


### Example

```javascript

const FileType = require('file-type');
const fs = require('fs');
var addon = require('wework-chat-node');

// 创建 sdk 对象
var wework = new addon.WeWorkChat({
  corpid: "corpid", // 企业id
  secret:"secret",
  private_key: "private key", // 私钥，用于消息解密
  seq: 880, // 数据拉取index,第一次从0开始
});

// 获取媒体文件 buffer 数据,不使用回调函数调用方式
const GetAndSaveImageFile = async (param,bufs)=>{
  if (!bufs){
   	bufs = [];
   }
  /*
   该方法提供了回调函数调用和直接返回值调用
  eg：
  {
      is_finished: true, // 文件数据是否获取完毕，如果没有，应该继续获取
      buf_index: "range:abc", // 当文件分片读取时，一次获取媒体文件的 index
      data: Buffer, // 文件buffer
  }
  */
  const data = wework.getMediaData(param)
  const bufVal = Buffer.from(data.data);
  bufs.push(bufVal);

  if (!data.is_finished){ // 分片读写,为了防止大文件 buffer 撑爆，建议使用 stream append 方式写文件
    param.index_buf = data.buf_index;
    GetAndSaveImageFile(param,bufs);
  }else {
    const bufVal = Buffer.concat(bufs);
    const fileProp = await FileType.fromBuffer(bufVal);
    fileName = `${Math.random().toString(36).substring(3)}.${fileProp.ext}`
    fs.createWriteStream(fileName).write(bufVal);
  }
}

// 使用回调函数方式获取媒体文件
const callSaveImage = async(err,data)=>{
  console.log(data);
  const bufVal = Buffer.from(data.data);
  const fileProp = await FileType.fromBuffer(bufVal);
  fileName = `${Math.random().toString(36).substring(3)}.${fileProp.ext}`
  fs.createWriteStream(fileName).write(bufVal);
}

const testGetMediaData = ()=>{
   let param = {
        sdk_fileid:"test_sdk_fileid",
        index_buf:"",
      };
      wework.getMediaData(param,callSaveImage);
}

testGetMediaData();


// 获取聊天数据回调函数
const callData = async (msg)=>{
  console.log("js msg:",msg);
  try {
    const msgObj = JSON.parse(msg);
    const msgType = msgObj.msgtype;
    if (msgType=="image") {
      let param = {
        sdk_fileid:msgObj.image.sdkfileid,
        index_buf:"",
      }
      //wework.getMediaData(param, callSaveImage)
     await GetAndSaveImageFile(param);
    }
  } catch (error) {
    console.log("parse data error:",error);
  }
}

void async function() {
      // 拉取聊天数据，参数为一个回调函数，聊天数据实时通过此回调函数返回,函数异步执行
    console.log(await wework.fetchData(callData));
}()

setTimeout(() => {
    console.log('I want to stop fetching data.');
    // 手动优雅结束拉取消息，并返回当前seq
    const seq = wework.stopFetch();
    console.log('end seq:', seq);
}, 60 *1000);

```

### TO DO
* 获取媒体文件支持 async(异步) 事件监听方式获取数据流。
* 获取聊天数据支持事件监听方式获取。
* 暴露单独获取获取会话记录数据、SDK解密接口，由调用者自己控制获取数据频率等流程。
