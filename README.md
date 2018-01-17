# CoAP: Constrained Application Protocol (RFC 7252)  
This coap repository is based on `master` branch `@b425b150` commit of [libcoap](https://github.com/obgm/libcoap/tree/b425b150c2fdf0d24810bb6af55db4f6d4fb65b8).   
For further information related to CoAP, see http://coap.technology.  

## How to Use in rt-thread  

### 1 Get `CoAP` Package  
CoAP package Path in `menuconfig` :   
`RT-Thread online packages/IoT/CoAP`.  

### 2 Open MSH Support  
Config in `menuconfig` :   
`RT-Thread Components/Command shell/Using mode shell`  

Modify MSH consle buffer in `menuconfig` :   
Set `RT-Thread Kernel/Kernel Device Object/the buffer size for console log printf` as `512`      

### 3 Compile and Download  
If there are some `errors` after compiled , please refer to the following [Notes](#Notes) 

### 4 Run Example   
Use coap_client to request `coap.me/test`  

#### 4.1 coap client example  
Using `-m` param to select coap client request method , it only realize `get` method.  
```
msh />coap_client -m get coap://coap.me/test
uri.path.s = test; uri.host.s = coap.me/test 
server_host = coap.me
DNS lookup succeeded. IP=134.102.218.18
welcome to the ETSI plugtest! last change: 2018-01-15 13:55:19 UTC
```

#### 4.2 coap server example  

```
msh />coap_server 
```
Default coap server uri is `coap://yourserveripaddr/rtthread` , use another coap_client to request this server as `coap_client -m get coap://yourserveripaddr/rtthread` , then client will return `Hello rtthread!` message.   

<span id="Notes"></span>
## Notes  
### 1 `strings.h` no define problem  

 Modify `libcoap/include/coap/encode.h` at line 13 as `#if (BSD >= 199103) || defined(WITH_CONTIKI) || defined(RTTHREAD_VERSION)`  

### 2 `sys/select.h` or `sys/socket.h` can't found problem    

Config in `menuconfig` :   
Don't select `RT-Thread Component/Device virtual file system/Enable BSD socket/Toolchain sys/select.h` and `Toolchain sys/socket.h`.  

### 3 `FD_SETSIZE` too small problem  
Config in `menuconfig` :   
The `RT-Thread Component/Device virtual file system/The maximal number of opened files` value need to  greater or equal to `RT-Thread Component/Network stack/light weight TCP/IP stack/The number of raw connection` value.  

## Reference    
1 CoAP Official website: [http://coap.technology/](http://coap.technology/)  
2 CoAP test server: [coap.me](http://coap.me/)  
   