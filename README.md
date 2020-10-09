# 诸葛C++ SDK

[TOC]

诸葛C++ SDK适应于通过C++开发的Windows、Mac、Linux平台的桌面应用程序。适应的语言标准为C++ 11，对于Windows平台，最低可兼容至Visual Studio 2013。

## 集成

### 通过源码方式集成

通过源码方式进行集成只需要将本工程include目录中的头文件拷贝到项目所依赖的头文件目录，将src目录中的文件拷贝到项目的源码目录即可。

### 通过静态链接库的方式集成

开发者需要根据自己的开发平台，自行编译静态链接库，将本工程头文件添加到目标项目的头文件目录，并将静态链接库添加到项目依赖中。

## 使用

### 初始化

在使用SDK之前，需要对其进行初始化。初始化的时机，建议选择在应用程序自身启动初始化时进行。初始化操作只能进行一次，如果多次进行初始化，会抛出异常。

#### 默认初始化

```c++
#include "zhuge_sdk.h"

int main()
{
  const std::string api_host = "54.222.139.120";
  const int api_port = 8081;
  const std::string app_key = "xxxxxxx";
  
  // 初始化SDK配置
  zhugeio::init_zhuge_sdk(api_host, api_port, app_key);
  
  return 0;
}
```

默认初始化只需要三个参数即可：

* `api_host` API服务器地址
* `api_port` API服务器端口 (初次调试，建议先选择非HTTPS的端口，关于SSL的集成，会在后续小节专题介绍)
* `app_key` App Key，需要咨询诸葛的技术支持人员获得

#### 自定义初始化

通常情况下，默认初始化的参数就可以满足大多数开发场景的需要。如果有特殊的需求，比如开启实时调试功能、调整数据上传频率等，就可以通过`ZhugeSDKConfig`对象来配置更多的参数：

```c++
#include "zhuge_sdk.h"

int main()
{
  const std::string api_host = "54.222.139.120";
  const int api_port = 8081;
  const std::string app_key = "xxxxxxx";
  
  // 初始化SDK配置
  zhugeio::ZhugeSDKConfig zhuge_sdk_config(api_host, api_port, app_key);
  zhuge_sdk_config
		.APIPath("/apipool")
		.Platform(zhugeio::ZHUGE_PLATFORM_JS)
		.ProcessIntervalMilliseconds(3000)
		.MaxSendSize(10)
		.EnableLog(true)
		.EnableDebug(false)
		.TimeZone(28800000)
		.UserDeviceID("")
		.APIConnectionTimeout(10)
		.APIReadTimeout(5)
		.APIWriteTimeout(10)
		.MaxStorageRecords(1000);
  zhugeio::init_zhuge_sdk(zhuge_sdk_config);
  
  return 0;
}
```

* `APIPath`  数据上传API路径，默认为`/apipool`
* `Platform` 数据上传平台，默认为js
* `ProcessIntervalMilliseconds` 数据上传处理周期，默认为3000ms
* `MaxSendSize` 一次网络通信最多上传事件数，SDK会将在数据上传处理周期内收到的事件按照该事件数打包成一个整体进行上传，从而减少网络通信的开销，如果超过了该事件数，则会拆分为多次网络通信进行上传。默认为10。
* `EnableLog` 是否开启调试日志，默认为false，但建议在与诸葛技术支持人员进行初次对接调试时，将该配置项打开。调试日志会默认输出到`std::clog` 对象当中。
* `EnableDebug` 是否开启实时调试，默认为false。
* `TimeZone` 设置时区偏移量，默认为28800000，即北京时间东八区。
* `UserDeviceID` 由开发者自己来指定标识当前设备的唯一ID，如果不指定，SDK会根据目前所处的系统，来自动生成一个设备ID。默认为空，采用系统自动生成的ID。
* `APIConnectionTimeout` API建立连接超时时间，单位为秒。默认10秒。
* `APIReadTimeout` 等待API响应时间，单位为秒，默认为5秒。
* `APIWriteTimeout` API上传数据超时时间，单位为秒，默认为10秒。
* `MaxStorageRecords` 本地上传队列最大存储记录数。当网络发生故障时，上传失败的记录会被保留在SDK的上传队列中，然后会按固定时间间隔，即`ProcessIntervalMilliseconds`进行重试。为了避免队列中的数据不断增长，占据过多的存储空间，需要为其指定一个上限，当超过上限时，会从队列中删除1/4的旧数据。默认上限为1000条数据。

除了`ZhugeSDKConfig` 构造函数所需要的配置参数，以上配置参数都是可选的，SDK提供了默认值，只有在开发者认为默认值不适合的时候才需要替换。

### 设备ID

默认情况下，诸葛会给每个设备分配一个设备ID来追踪用户，如果需要自己定义设备ID追踪用户，或者所在的硬件平台生成设备ID有问题，就可以通过配置`UserDeviceID`来取代SDK自动采集的设备ID。

诸葛SDK在各个平台采集的设备ID渠道：

#### Windows

采集自注册表项：`HKLM\System\CurrentControlSet\Control\IDConfigDB\Hardware Profiles`  中的`HwProfileGuid`项目

#### MacOS

采集自硬件UUID，点击关于本机 -> 概览 -> 系统报告 -> 硬件 与该项中的硬件UUID一致。

#### Linux

读取自`/etc/machine-id` 文件中的设备唯一ID。

可通过SDK的方法来获取当前所使用的设备ID：

```c++
const std::string& device_id = zhugeio::zhuge_sdk->GetDeviceID();
std::clog << "The device id: " << device_id <<std::endl;
```

### 上传用户属性

诸葛SDK提供了对用户属性进行追踪的API，该API需要开发者提供一个用户的唯一标识，比如用户注册的唯一账号、邮箱或者手机号。同时也允许记录用户更多的属性信息，从而对用户拥有更多的了解。

```c++
zhugeio::ZhugeUser* user = new zhugeio::ZhugeUser("sam@gmail.com");
user->AddCustomProperty("name", "SamChi7");
user->AddCustomProperty("age", 100);
zhugeio::zhuge_sdk->Identify(user);
```

* `ZhugeUser` 对象用来表示一个用户的完整信息。构造函数接收一个唯一的用户标识。
* `AddCustomProperty` 方法可以向用户信息中添加一个自定义属性，比如姓名、年龄、是否是VIP等等。
* `Identify` 方法会将这些组装好的用户属性提交到发送队列。

**注意：不要自行delete上传数据对象，SDK会在发送数据之后自动对其所占用的内存进行释放。如果自行delete了上传数据对象，将会引发未知的错误**

### 上传设备信息

```c++
zhugeio::ZhugePlatform* platform = zhugeio::zhuge_sdk->GetPlatformInfo();
zhugeio::zhuge_sdk->Platform(platform);
```

通过`GetPlatformInfo()`方法，SDK会自动采集诸如OS类型、语言、分辨率等设备平台信息(目前只有Windows平台会采集这些信息)，然后通过`Platform()` 方法上传到诸葛分析平台。

您也可以在`GetPlatform()` 方法执行之后，自行添加通过操作系统API获取的设备信息：

```c++
zhugeio::ZhugePlatform* platform = zhugeio::zhuge_sdk->GetPlatformInfo();
platform->AddCustomProperty("_网络信号", "5G");
zhugeio::zhuge_sdk->Platform(platform);
```

### 会话

您可以根据您的业务特点来自行定义一次会话开始和结束。比如可以将应用启动到关闭作为一段会话，也可以将应用程序从前台活跃到后台不活跃作为一次会话。SDK为您提供了标记会话开始和结束的接口，您可以在不同的钩子中(比如在应用启动时调用开启会话接口，在应用关闭之时调用关闭会话接口)进行调用。

```c++
// 开启会话
zhugeio::zhuge_sdk->StartSession();

// 获取当前会话ID，如果没有开启会话，则会返回0
const long long session_id = zhugeio::zhuge_sdk->GetCurrentSessionID();

// 结束会话
zhugeio::zhuge_sdk->StopSession();
```

### 上传事件

您可以在希望记录用户行为的位置，自定义事件：

```c++
zhugeio::ZhugeEvent* event = new zhugeio::ZhugeEvent("提交订单");
event->AddCustomProperty("商品数目", 100);
event->AddCustomProperty("商品总价", "250.00");
zhugeio::zhuge_sdk->Track(event);
```

* 通过`ZhugeEvent`类型构建一个事件属性对象，其中构造函数参数为事件名称。
*  `AddCustomProperty` 方法可以向事件属性中添加一个自定义属性。
* `Track` 方法可以将组装好的事件属性对象提交到任务队列。

#### 设置公共事件属性

有些属性可能是需要在每个事件中都会出现的，每次重复设置就会比较麻烦。可以通过设置公共属性的方式，这样每次提交事件，SDK都会自动对这些属性进行设置。

```c++
Json::Value common_properties;
common_properties["common_property_1"] = 1;
common_properties["common_property_2"] = "2";
common_properties["common_property_3"] = 3.0;
zhugeio::zhuge_sdk->SetCommonEventCustomProperties(common_properties);
```

这些属性是可以被覆盖的，如果在在公共事件属性中设置了某个属性值，在后续的事件对象中再次设置了该属性的值，则事件中设置的值会覆盖掉公共属性中的值。

#### 事件时长统计

如果您希望统计一个事件发生的时长，比如视频的播放，页面的停留，那么可以调用如下接口来进行：

```c++
// 首先，构建目标事件对象
zhugeio::ZhugeEvent* play_event = new zhugeio::ZhugeEvent("播放视频");
play_event->AddCustomProperty("视频标题", "社会主义精神文明建设");

// 通过StartTrack方法开始计时，该方法会返回一个TrackTimeHolder对象，需要保留该对象到事件结束
zhugeio::TrackTimeHolder holder = zhugeio::zhuge_sdk->StartTrack(play_event);

// Do something others ...

// 结束事件
zhugeio::zhuge_sdk->EndTrack(holder);
```

### 关闭SDK

建议您在应用程序结束时，通过系统的关闭钩子，来显式的对诸葛SDK进行关闭。SDK在关闭时会进行两个操作：

* 将目前数据上传队列中残存的数据通过网络进行上传。
* 回收SDK所占有的资源，并对`zhugeio::zhuge_sdk` 对象进行析构。

关闭操作：

```c++
zhugeio::shutdown_zhuge_sdk();
```

该操作会阻塞到所有数据均上传完毕以及资源释放为止。

而为了避免阻塞时间过长带来糟糕的退出体验，您也可以指定一个超时时间，单位为毫秒：

```c++
zhugeio::shutdown_zhuge_sdk(5000);  // 最多阻塞5秒
```

当SDK被关闭后，再通过Identify、Track、Platform等方法上传数据，数据会被直接丢弃，不会再被加入到上传队列。

## 注意事项

### 字符编码

诸葛平台默认采用UTF-8字符编码，而在有些平台中，比如Windows，可能更多采用的是GBK编码，因此，在上传中文的事件名或属性名称时，就需要对编码进行一些转换，将GBK转换为UTF-8。诸葛SDK内置了这种转换工具：

```c++
zhugeio::ZhugeEvent* event = new zhugeio::ZhugeEvent(
  zhugeio::GBK_TO_UTF8("加入购物车"));
event->AddCustomProperty(zhugeio::GBK_TO_UTF8("商品数目"), 1);
event->AddCustomProperty(zhugeio::GBK_TO_UTF8("商品价格"), "100.00");
zhugeio::zhuge_sdk->Track(event);
```

### 集成OpenSSL

通常，为了确保数据的安全，我们需要通过HTTPS的方式来上传数据。这就需要我们集成OpenSSL，用来对上传的数据进行加密。

#### Visual Studio的集成

首先，需要到https://slproweb.com/products/Win32OpenSSL.html下载1.1.1版本的32位OpenSSL安装包，进行安装，安装完毕之后：

将`C:\Program Files (x86)\OpenSSL-Win32\include` 目录添加至VS工程的Include目录中。

将`C:\Program Files (x86)\OpenSSL-Win32\lib` 目录添加至VS工程的lib目录中。

将以下库加入到linker的input中：

```
ws2_32.lib
crypt32.lib
libcrypto.lib
libssl.lib
```

在SDK源码文件`zhuge_sdk.cpp`的最上方，加入宏定义：

```c++
#define CPPHTTPLIB_OPENSSL_SUPPORT
```

将SDK初始化的API地址和端口改为支持HTTPS的地址和端口

重新编译运行，系统就能支持以HTTPS的方式上传数据了。

### 异步上传与线程安全

SDK均采用异步上传的方式，当通过诸如Identify、Track、Platform、StartSession、StopSession、StartTrack、EndTrack等方法时，会将上传数据加入到发送队列中，而不会立即通过网络请求进行上传，而是通过后台专门的数据上传线程择机进行上传。

因此，调用这些方法所花费的时间是微不足道的，不会对您的业务产生阻塞。

另外，这些方法也都是线程安全的，可在多个线程同时调用，无需额外进行加锁。

### 断网重传

如果SDK在上传数据的过程中不幸发生了网络故障，那么这些上传失败的数据会被保留在上传队列，SDK会按照时间间隔不断进行重试，直到网络故障恢复。

而为了避免上传队列中的数据无限占用太多内存资源，会对队列中上传数据的条数做出限制，默认最多保留1000条，当超过此限制，会自动丢弃1/4的旧数据。通过`MaxStorageRecords` 初始化选项可以改变保留的最大数据条数。

需要注意：这里的数据条数并不等同于事件数，SDK会将多个事件打包成一条记录进行网络传送，这里的数据条数是指这种已经打包的网络传送记录数目。因此，1000条上传记录所包含的事件数通常要大于1000个事件，其所包含的数据量在1000个事件到1000 * `MaxSendSize`之间。