#include <iostream>
#include <thread>
#include "zhuge_sdk.h"



int main()
{
	const std::string api_host = "54.222.139.120";
	const int api_port = 8081;
	const std::string app_key = "080163d942594de09e5fcb61d4d22544";

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

	// 获取设备ID
	// const std::string& device_id = zhugeio::zhuge_sdk->GetDeviceID();
	// std::clog << "The device id: " << device_id << std::endl;

	// 用户属性上传
	zhugeio::ZhugeUser* user = new zhugeio::ZhugeUser("chihongze7@gmail.com");
	user->AddCustomProperty("name", "SamChi7");
	user->AddCustomProperty("age", 100);
	zhugeio::zhuge_sdk->Identify(user);

	// 自动获取并上传设备信息：OS、语言、分辨率
	zhugeio::ZhugePlatform* platform = zhugeio::zhuge_sdk->GetPlatformInfo();
	zhugeio::zhuge_sdk->Platform(platform);

	// 开启会话
	zhugeio::zhuge_sdk->StartSession();

	// 获取当前会话ID
	// const long long session_id = zhugeio::zhuge_sdk->GetCurrentSessionID();
	// std::cout << "Current session id: " << session_id << std::endl;

	// 设置公共属性
	Json::Value common_properties;
	common_properties["common_property_1"] = 1;
	common_properties["common_property_2"] = "2";
	common_properties["common_property_3"] = 3.0;
	zhugeio::zhuge_sdk->SetCommonEventCustomProperties(common_properties);

	// 事件属性上传
	for (int i = 0; i < 3; i++) {
		zhugeio::ZhugeEvent* event = new zhugeio::ZhugeEvent(zhugeio::GBK_TO_UTF8("Add_cart"));
		event->AddCustomProperty(zhugeio::GBK_TO_UTF8("Shopping_num"), i);
		event->AddCustomProperty("common_property_3", "4.0");
		zhugeio::zhuge_sdk->Track(event);
	}

	// 事件计时
	zhugeio::ZhugeEvent* play_event = new zhugeio::ZhugeEvent(zhugeio::GBK_TO_UTF8("Play_video"));
	play_event->AddCustomProperty(zhugeio::GBK_TO_UTF8("Video_title"), "ssss");
	zhugeio::TrackTimeHolder holder = zhugeio::zhuge_sdk->StartTrack(play_event);
	zhugeio::zhuge_sdk->EndTrack(holder);

	// 结束会话
	zhugeio::zhuge_sdk->StopSession();

	// 关闭SDK
	zhugeio::shutdown_zhuge_sdk();

	std::cin.get();

	return 0;
}
