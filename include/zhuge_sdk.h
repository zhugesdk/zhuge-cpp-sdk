#ifndef ZHUGE_SDK_H_
#define ZHUGE_SDK_H_
#include <iostream>
#include <string>
#include <atomic>
#include <queue>
#include <list>
#include <set>
#include <future>
#include <mutex>
#include <random>
#include <ostream>
#include <sstream>
#include <condition_variable>
#include <exception>
#include "json.h"

namespace zhugeio
{

	static const char* ZHUGE_SDK_VERSION = "2.3.2";

	// 诸葛数据上传平台类型
	static const char* ZHUGE_PLATFORM_JS = "js";  // web平台
	static const char* ZHUGE_PLATFORM_ANDROID = "and";  // 安卓平台
	static const char* ZHUGE_PLATFORM_IOS = "ios";  // iOS平台

	// 诸葛SDK上传数据类型
	const char* const ZG_EVT = "evt";  // 事件类型数据
	const char* const ZG_USR = "usr";  // 用户类型数据
	const char* const ZG_PL = "pl";    // 平台类型数据
	const char* const ZG_SS = "ss";    // 会话开始类型数据
	const char* const ZG_SE = "se";    // 会话结束类型数据

	// 默认配置常量
	static const std::string DEFAULT_API_PATH = "/apipool";
	static const int DEFAULT_PROCESS_INTERVAL_MILLISECONDS = 3000;
	static const int DEFAULT_MAX_SEND_SIZE = 10;
	static const bool DEFAULT_ENABLE_LOG = false;
	static const bool DEFAULT_ENABLE_DEBUG = false;
	static const int DEFAULT_TIME_ZONE = 28800000;
	static const int DEFAULT_API_CONNECTION_TIMEOUT_S = 10;
	static const int DEFAULT_API_READ_TIMEOUT_S = 5;
	static const int DEFAULT_API_WRITE_TIMEOUT_S = 10;
	static const unsigned int DEFAULT_MAX_STORAGE_RECORDS = 1000;

	// SDK配置构造者
	class ZhugeSDKConfig
	{
	private:
	public:
		// 数据上传API Host
		const std::string api_host;

		// 数据上传API Port
		const int api_port;

		// 数据上传API Path
		std::string api_path;

		// App key
		const std::string app_key;

		// 平台
		std::string platform;

		// 后台任务处理时间间隔
		int process_interval_milliseconds;

		// 一次网络通信最大上传事件数
		int max_send_size;

		// 日志开关
		bool enable_log;

		// Debug开关
		bool enable_debug;

		// 用户指定的设备ID
		std::string user_device_id;

		// 时区
		int time_zone;

		// API连接创建超时时间
		int api_connection_timeout;

		// API返回超时时间
		int api_read_timeout;

		// API提交超时时间
		int api_write_timeout;

		// 上传数据最大存储记录数
		unsigned int max_storage_records;

		// 上传数据保存文件
		std::string storage_file_path;

		ZhugeSDKConfig(
			const std::string api_host,
			const int api_port,
			const std::string app_key
			);

		ZhugeSDKConfig& APIPath(std::string api_path);

		ZhugeSDKConfig& Platform(std::string platform);

		ZhugeSDKConfig& ProcessIntervalMilliseconds(const int process_interval_milliseconds);

		ZhugeSDKConfig& MaxSendSize(const int max_send_size);

		ZhugeSDKConfig& EnableLog(const bool enable_log);

		ZhugeSDKConfig& EnableDebug(const bool enable_debug);

		ZhugeSDKConfig& UserDeviceID(const std::string user_device_id);

		ZhugeSDKConfig& TimeZone(int time_zone);

		ZhugeSDKConfig& APIConnectionTimeout(const int api_connection_timeout);

		ZhugeSDKConfig& APIReadTimeout(const int api_read_timeout);

		ZhugeSDKConfig& APIWriteTimeout(const int api_write_timeout);

		ZhugeSDKConfig& MaxStorageRecords(const unsigned int max_storage_records);

		ZhugeSDKConfig& StorageFilePath(const std::string storage_file_path);

		friend std::ostream& operator<<(std::ostream& out, ZhugeSDKConfig config);
	};

	// 诸葛SDK异常
	class ZhugeSDKException : public std::exception
	{
	private:
		std::string msg;
	public:
		ZhugeSDKException(std::string msg)
		{
			this->msg = msg;
		}

		virtual const char* what() const _NOEXCEPT
		{
			std::clog << "诸葛SDK调用异常：'" << this->msg << "'" << std::endl;
			return this->msg.c_str();
		}
	};

	// 上传数据表示
	class ZhugeSDKUploadData
	{
	private:
		const char* const data_type;
		Json::Value* data;
	public:
		// 默认构造函数
		ZhugeSDKUploadData(const char* const data_type);

		// 获取上传数据类型
		inline const char* GetDataType()
		{
			return data_type;
		}

		// 判断某个系统属性是否存在
		bool HasSystemProperty(const std::string& property_name);

		// 判断某个自定义属性是否存在
		bool HasCustomProperty(const std::string& property_name);

		// 获取某个系统属性的值，如果不存在，则返回默认值
		std::string GetSystemStringProperty(
			const std::string& property_name, std::string default_value)
		{
			if (this->HasSystemProperty(property_name)) {
				return (*(this->data))["pr"]['$' + property_name].asString();
			}
			else {
				return default_value;
			}
		}

		// 获取某个自定义属性的值，如果没有，则返回默认值
		std::string GetCustomProperty(
			const std::string& property_name, std::string default_value)
		{
			if (this->HasCustomProperty(property_name)) {
				return (*(this->data))["pr"]['_' + property_name].asString();
			}
			else {
				return default_value;
			}
		}

		// 添加系统属性，以$开头
		template <typename PROP_TYPE>
		void AddSystemProperty(const std::string& property_name, PROP_TYPE value)
		{
			(*(this->data))["pr"]['$' + property_name] = value;
		}

		// 添加自定义属性，以_开头
		template <typename PROP_TYPE>
		void AddCustomProperty(const std::string& property_name, PROP_TYPE value)
		{
			(*(this->data))["pr"]['_' + property_name] = value;
		}

		// 如果属性不存在，则添加属性
		template <typename PROP_TYPE>
		void AddSystemPropertyIfAbsent(const std::string& property_name, PROP_TYPE value)
		{
			const std::string new_member = '$' + property_name;
			if (!(*(this->data))["pr"].isMember(new_member)) {
				(*(this->data))["pr"][new_member] = value;
			}
		}

		// 如果属性不存在，则添加属性
		template <typename PROP_TYPE>
		void AddCustomPropertyIfAbsent(const std::string& property_name, PROP_TYPE value)
		{
			const std::string new_member = '_' + property_name;
			if (!(*(this->data))["pr"].isMember(new_member)) {
				(*(this->data))["pr"][new_member] = value;
			}
		}

		virtual const Json::Value& GetJSONData();

		virtual std::string ToJSON();

		// 析构函数
		virtual ~ZhugeSDKUploadData();
	};

	// 用户数据
	class ZhugeUser : public ZhugeSDKUploadData
	{
	public:
		ZhugeUser(const std::string& user_id);
		virtual ~ZhugeUser();
		inline const std::string& GetUserId()
		{
			return this->user_id;
		}
	private:
		std::string user_id;
	};

	// 平台数据
	class ZhugePlatform : public ZhugeSDKUploadData
	{
	public:
		ZhugePlatform();
		virtual ~ZhugePlatform();
	};

	// 事件数据
	class ZhugeEvent : public ZhugeSDKUploadData
	{
	public:
		ZhugeEvent(const std::string& event_name);
		virtual ~ZhugeEvent();
	};

	// 会话开始
	class ZhugeSessionStart : public ZhugeSDKUploadData
	{
	public:
		ZhugeSessionStart(const long long sid);
		virtual ~ZhugeSessionStart();
	};

	// 会话结束
	class ZhugeSessionEnd : public ZhugeSDKUploadData
	{
	public:
		ZhugeSessionEnd(const long long sid);
		virtual ~ZhugeSessionEnd();
	};

	// SDK任务处理队列
	template <class T>
	class ZhugeSDKTaskQueue
	{
	private:
		std::queue<T> queue;
		mutable std::mutex mutex;
		std::condition_variable cond;
	public:
		ZhugeSDKTaskQueue()
		{

		}

		bool empty()
		{
			return queue.empty();
		}

		void Enqueue(T data)
		{
			std::lock_guard<std::mutex> lock(mutex);
			queue.push(data);
		}

		const T Dequeue()
		{
			std::unique_lock<std::mutex> lock(mutex);
			if (queue.empty()) {
				return nullptr;
			}
			T data = queue.front();
			queue.pop();
			return data;
		}

		void DequeueToBuffer(std::list<T>& buffer)
		{
			std::unique_lock<std::mutex> lock(mutex);
			while (!queue.empty()) {
				T data = queue.front();
				queue.pop();
				buffer.push_back(data);
			}
		}

		~ZhugeSDKTaskQueue()
		{

		}
	};

	class ZhugeSDK;

	// SDK上传数据存储
	class SDKDataStorage
	{
	protected:
		ZhugeSDK* sdk;
		SDKDataStorage(ZhugeSDK* sdk);
	public:
		virtual void Save(std::string& data) = 0; // 保存上传数据到存储
		virtual std::list<std::string>& Load() = 0;  // 加载保存的数据
		virtual void Sync() = 0;  // 同步操作后的缓冲数据
		virtual ~SDKDataStorage(){};
	};

	// 基于内存的SDK上传数据存储
	class MemorySDKDataStorage : public SDKDataStorage
	{
	private:
		std::list<std::string> buffer;
		void ResizeBuffer();
	public:
		MemorySDKDataStorage(ZhugeSDK* sdk);
		virtual void Save(std::string& data);
		virtual std::list<std::string>& Load();
		virtual void Sync();
	};

	// 基于文件的SDK上传数据存储
	class FileSDKDataStorage : public SDKDataStorage
	{
	private:
		std::list<std::string> buffer;
		std::string last_read_file;
		void GetFileList(std::set<std::string> &files);
	public:
		FileSDKDataStorage(ZhugeSDK* sdk);
		virtual void Save(std::string& data);
		virtual std::list<std::string>& Load();
		virtual void Sync();
	};

	// 后台任务处理线程逻辑
	class ZhugeSDKTaskProcess
	{
	private:
		ZhugeSDK* zhuge_sdk;
		ZhugeSDKTaskQueue<ZhugeSDKUploadData*> upload_data_queue;  // 数据上传队列
		SDKDataStorage* data_storage;
		std::atomic<bool> stop_mark;
		std::promise<void> shutdown_promise;
		std::list<ZhugeSDKUploadData*> upload_data_buf;
		void Process();
		void HandleUploadData();
		void BuildFullUploadData(Json::Value& root);
		void TransDataWithAPI();
	public:
		ZhugeSDKTaskProcess(ZhugeSDK* zhuge_sdk);
		void AddUploadDataToQueue(ZhugeSDKUploadData* upload_data);
		void Run();
		void Stop(int timeout);
		void Stop();
		~ZhugeSDKTaskProcess();
	};

	struct TrackTimeHolder
	{
		ZhugeEvent* event_ptr;
		const long long begin_time;
	};

	class ZhugeSDK
	{
	private:

		// 当前SDK是否已经被暂停
		std::atomic<bool> stopped;

		// 系统自动生成的设备ID
		std::string auto_device_id;

		// 当前用户ID
		std::string user_id;

		// 数据上传任务
		ZhugeSDKTaskProcess* upload_process;

		// 当前会话ID
		std::atomic<long long> session_id;

		// 通用系统属性
		Json::Value common_system_properties;

		// 通用自定义属性
		Json::Value common_custom_properties;

		// 平台相关信息
		ZhugePlatform* platform_info;

		// 会话操作相关锁
		mutable std::mutex session_mutex;

		// 基于不同的平台，计算出不同的设备ID
		std::string GenDeviceID();

		// 填充公共属性
		inline void FillCommonEventProperties(ZhugeSDKUploadData* data_ptr)
		{
			auto system_members = this->common_system_properties.getMemberNames();
			for (auto member : system_members) {
				data_ptr->AddSystemPropertyIfAbsent(member, common_system_properties[member]);
			}

			auto cus_members = this->common_custom_properties.getMemberNames();
			for (auto member : cus_members) {
				data_ptr->AddCustomPropertyIfAbsent(member, common_custom_properties[member]);
			}
		}

	public:
		// SDK配置
		ZhugeSDKConfig* sdk_config;

		// 初始化SDK所需要的配置属性
		ZhugeSDK(ZhugeSDKConfig* zhuge_sdk_config);

		// 初始化设备ID并启动后台处理进程
		void StartProcess();

		// 获取设备唯一ID
		const std::string& GetDeviceID();

		// 设置通用的事件系统属性
		void SetCommonEventSystemProperties(Json::Value& system_properties);

		// 设置通用的事件自定义属性
		void SetCommonEventCustomProperties(Json::Value& custom_properties);

		// 上传用户数据
		void Identify(ZhugeUser* user_ptr);

		// 清除当前用户ID
		void CleanUserId();

		// 自动采集设备信息
		ZhugePlatform* GetPlatformInfo();

		// 上传设备信息
		void Platform(ZhugePlatform* platform_ptr);

		// 开启会话
		void StartSession();

		// 结束会话
		void StopSession();

		// 获取当前会话ID
		inline const long long GetCurrentSessionID()
		{
			return this->session_id.load();
		}

		// 上传事件数据
		void Track(ZhugeEvent* event_ptr);

		// 开始事件计时
		const TrackTimeHolder StartTrack(ZhugeEvent* event_ptr);

		// 结束事件计时
		void EndTrack(TrackTimeHolder& track_time_holder);

		// 终止SDK执行
		// 系统会执行Flush操作，并回收相关资源
		void Shutdown();

		// 终止SDK执行，带有指定超时时间
		void Shutdown(int timeout);

		~ZhugeSDK();
	};

	extern ZhugeSDK* zhuge_sdk;

	// SDK进行初始化
	void init_zhuge_sdk(
		const std::string& api_host, const int api_port, const std::string& app_key);

	void init_zhuge_sdk(ZhugeSDKConfig& zhuge_sdk_config);

	// 关闭全局默认SDK对象
	void shutdown_zhuge_sdk();

	// 关闭全局默认SDK对象，并指定超时时间
	void shutdown_zhuge_sdk(int timeout);

	std::string GBK_TO_UTF8(const std::string &source);
}
#endif
