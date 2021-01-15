// #define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <cstdio>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <ctime>
#include <exception>
#include <set>
#include <sstream>
#include "zhuge_sdk.h"

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <IOKit/IOKitLib.h>
#include <dirent.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif


namespace zhugeio
{
	ZhugeSDKUploadData::ZhugeSDKUploadData(const char* const data_type) : data_type(data_type)
	{
		this->data = new Json::Value();
		(*data)["dt"] = data_type;
	}

	bool ZhugeSDKUploadData::HasSystemProperty(const std::string& property_name)
	{
		return (*(this->data))["pr"].isMember('$' + property_name);
	}

	bool ZhugeSDKUploadData::HasCustomProperty(const std::string& property_name)
	{
		return (*(this->data))["pr"].isMember('_' + property_name);
	}

	const Json::Value& ZhugeSDKUploadData::GetJSONData()
	{
		return *(this->data);
	}

	std::string ZhugeSDKUploadData::ToJSON()
	{
		return data->toStyledString();
	}

	ZhugeSDKUploadData::~ZhugeSDKUploadData()
	{
		delete this->data;
		this->data = nullptr;
	}

	ZhugeUser::ZhugeUser(const std::string& user_id) : ZhugeSDKUploadData(ZG_USR)
	{
		this->user_id = user_id;
		this->AddSystemProperty("cuid", user_id);
	}

	ZhugeUser::~ZhugeUser()
	{

	}

	ZhugeEvent::ZhugeEvent(const std::string& event_name) : ZhugeSDKUploadData(ZG_EVT)
	{
		this->AddSystemProperty("eid", event_name);
	}

	ZhugeEvent::~ZhugeEvent()
	{

	}

	ZhugeSessionStart::ZhugeSessionStart(const long long sid) : ZhugeSDKUploadData(ZG_SS)
	{
		this->AddSystemProperty("sid", sid);
	}

	ZhugeSessionStart::~ZhugeSessionStart()
	{

	}

	ZhugeSessionEnd::ZhugeSessionEnd(const long long sid) : ZhugeSDKUploadData(ZG_SE)
	{
		this->AddSystemProperty("sid", sid);
	}

	ZhugeSessionEnd::~ZhugeSessionEnd()
	{

	}

	ZhugePlatform::ZhugePlatform() : ZhugeSDKUploadData(ZG_PL)
	{

	}

	ZhugePlatform::~ZhugePlatform()
	{

	}

	SDKDataStorage::SDKDataStorage(ZhugeSDK* sdk) :
		sdk(sdk)
	{

	}

	MemorySDKDataStorage::MemorySDKDataStorage(ZhugeSDK* sdk) :
		SDKDataStorage(sdk)
	{

	}

	void MemorySDKDataStorage::ResizeBuffer()
	{
		// 调整数据存储上限，只保留上限3/4的数据
		if (buffer.size() >= this->sdk->sdk_config->max_storage_records) {
			const int remove_num = this->sdk->sdk_config->max_storage_records * 4;
			std::cout << "Resize buffer remove num: " << remove_num << std::endl;
			for (int i = 0; i < remove_num; i++)
			{
				buffer.pop_front();
			}
		}
	}

	void MemorySDKDataStorage::Save(std::string& data)
	{
		this->buffer.push_back(data);
		this->ResizeBuffer();
	}

	std::list<std::string>& MemorySDKDataStorage::Load()
	{
		this->ResizeBuffer();
		return this->buffer;
	}

	void MemorySDKDataStorage::Sync()
	{

	}

	FileSDKDataStorage::FileSDKDataStorage(ZhugeSDK* sdk) :
		SDKDataStorage(sdk)
	{

	}

	void FileSDKDataStorage::Save(std::string& data)
	{
		this->buffer.push_back(data);
	}

	std::list<std::string>& FileSDKDataStorage::Load()
	{
		std::set<std::string> files;
		this->GetFileList(files);  // 获取文件列表
		if (files.empty()) {
			if (this->sdk->sdk_config->enable_log) {
				std::cout << "[ZhugeSDK] There is no data files to load!" << std::endl;
			}
		} else {  // 有文件，从最新的文件中读取数据加入到buffer中
			const std::string fname = *(files.rbegin());
			if (this->sdk->sdk_config->enable_log) {
				std::clog << "[ZhugeSDK] Load upload data from " << fname << std::endl;
			}
			this->last_read_file = fname;
			std::ifstream input_file(
				this->sdk->sdk_config->storage_file_path + fname);
			if (input_file) {
				std::string line;
				while(std::getline(input_file, line)) {
					if (!line.empty()) {
						this->buffer.push_back(line);  // 写入缓冲
					}
				}
				input_file.close();
				if (this->sdk->sdk_config->enable_log) {
					std::clog 
						<< "[ZhugeSDK] Load upload data from " 
						<< fname 
						<< " finished." 
						<< std::endl;
				}
			} else {
				if (this->sdk->sdk_config->enable_log) {
					std::clog 
						<< "[ZhugeSDK] Read file " 
						<< fname 
						<< " error!" 
						<< std::endl;
				}
			}
		}
		return this->buffer;
	}

	void FileSDKDataStorage::Sync()
	{
		// 删除刚才加载的文件
		if (!this->last_read_file.empty()) {
			if (remove((
				this->sdk->sdk_config->storage_file_path + 
				this->last_read_file).c_str())) {
					if (this->sdk->sdk_config->enable_log) {
						std::clog 
							<< "[ZhugeSDK] Error deleting file: " 
							<< this->last_read_file 
							<< std::endl;
					}
			} else {
				if (this->sdk->sdk_config->enable_log) {
						std::clog 
							<< "[ZhugeSDK] Data file: "
							<< this->last_read_file
							<< " deleted!"
							<< std::endl;
				}
			}
		}

		// 将buffer中剩余的数据写入到新的文件
		if (!this->buffer.empty()) {
			using namespace std::chrono;
			const long long ts = duration_cast<milliseconds>(
				system_clock::now().time_since_epoch()).count();
			std::ostringstream ss;
			ss << sdk->sdk_config->storage_file_path << "zg" << ts;
			std::string filepath = ss.str();  // 基于时间戳，构建要写入的文件路径
			std::ofstream output_file(filepath);
			int count = 0;
			for (auto itr = buffer.begin(); itr != buffer.end(); itr++) {
				if (++count > 1000) {  // 超过1000条，写新文件
					output_file.close();
					std::this_thread::sleep_for(milliseconds(1));
					const long long ts = duration_cast<milliseconds>(
						system_clock::now().time_since_epoch()).count();
					std::ostringstream ss;
					ss << sdk->sdk_config->storage_file_path << "zg" << ts;
					std::string filepath = ss.str();  // 基于时间戳，构建要写入的文件路径
					output_file = std::ofstream(filepath);
					count = 0;
					if (this->sdk->sdk_config->enable_log) {
						std::clog 
							<< "[ZhugeSDK] New data file "
							<< filepath
							<< " created." 
							<< std::endl;
					}
				}
				output_file << (*itr) << std::endl;
			}
			output_file.close();
		}

		// 清空buffer所有数据
		this->buffer.clear();
		this->last_read_file = "";
	}

	void FileSDKDataStorage::GetFileList(std::set<std::string> &files)
	{
		std::string data_path = this->sdk->sdk_config->storage_file_path;
	#ifdef _WIN32
	#else
		DIR *dir;
		struct dirent *diread;
		if ((dir = opendir(data_path.c_str())) != nullptr) {
			while ((diread = readdir(dir)) != nullptr) {
				const std::string filename(diread->d_name);
				if (filename.rfind("zg", 0) == 0) {
					files.insert(filename);
				}
			}
			closedir(dir);
		} else {  // 读取失败
			if (this->sdk->sdk_config->enable_log) {
				std::clog 
					<< "Open data path " 
					<< data_path 
					<< " error!" 
					<< std::endl;
			}
		}
	#endif
	}

	ZhugeSDKTaskProcess::ZhugeSDKTaskProcess(ZhugeSDK* sdk) :
		zhuge_sdk(sdk),
		upload_data_queue()
	{
		this->stop_mark.store(false);
		if (sdk->sdk_config->storage_file_path.empty()) {
			this->data_storage = new MemorySDKDataStorage(sdk);
		}
		else {
			this->data_storage = new FileSDKDataStorage(sdk);  // TODO 添加文件存储实现
		}
	}

	void ZhugeSDKTaskProcess::BuildFullUploadData(Json::Value& root)
	{
		root["ak"] = zhuge_sdk->sdk_config->app_key;
		root["debug"] = zhuge_sdk->sdk_config->enable_debug ? 1 : 0;
		root["sln"] = "itn";
		root["owner"] = "zg";
		root["pl"] = zhuge_sdk->sdk_config->platform;
		root["sdk"] = "zg";
		root["sdkv"] = "2.0";
		root["tz"] = zhuge_sdk->sdk_config->time_zone;
		root["usr"]["did"] = zhuge_sdk->GetDeviceID();
		time_t t = time(0);
		char t_buf[255];
		strftime(t_buf, sizeof(t_buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
		root["ut"] = t_buf;
	}

	void ZhugeSDKTaskProcess::TransDataWithAPI()
	{
		try {
			std::list<std::string>& all_data = this->data_storage->Load();
			if (all_data.empty()) {
				return;
			}

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
			httplib::SSLClient cli(
				this->zhuge_sdk->sdk_config->api_host.c_str(), this->zhuge_sdk->sdk_config->api_port);
			cli.enable_server_certificate_verification(false);
#else
			httplib::Client cli(
				this->zhuge_sdk->sdk_config->api_host.c_str(), this->zhuge_sdk->sdk_config->api_port);
#endif

			// 设置API调用超时选项
			cli.set_connection_timeout(this->zhuge_sdk->sdk_config->api_connection_timeout, 0);
			cli.set_read_timeout(this->zhuge_sdk->sdk_config->api_read_timeout, 0);
			cli.set_write_timeout(this->zhuge_sdk->sdk_config->api_write_timeout, 0);

			// 执行上传
			httplib::Headers headers = {
				{ "User-Agent", "ZHUGE-CPP-SDK" },
				{ "Content-Type", "x-www-form-urlencode;charset=utf-8" }
			};

			for (auto itr = all_data.begin(); itr != all_data.end();) {
				std::string data = *itr;
				httplib::Params params;
				params.emplace("event", data);

				if (this->zhuge_sdk->sdk_config->enable_log) {
					std::clog << "[ZhugeSDK] Upload data: " << data << std::endl;
				}

				auto res = cli.Post(this->zhuge_sdk->sdk_config->api_path.c_str(), headers, params);

				if (res) {
					itr = all_data.erase(itr);
					if (this->zhuge_sdk->sdk_config->enable_log) {
						std::clog << "[ZhugeSDK] Upload Data success, status code: " << res->status << std::endl;
					}
				}
				else {
					itr++;
					if (this->zhuge_sdk->sdk_config->enable_log) {
						std::clog << "[ZhugeSDK] Upload error, error_code: " << res.error() << std::endl;
					}
				}
			}

			this->data_storage->Sync();  // 同步对数据存储的修改

		}
		catch (std::exception& e) {
			if (this->zhuge_sdk->sdk_config->enable_log) {
				std::clog << "[ZhugeSDK] Exception happened when upload data: " << e.what() << std::endl;
			}
		}
	}

	void ZhugeSDKTaskProcess::HandleUploadData()
	{
		// 将任务队列中的数据尽快提取到本地，减少锁对埋点方法的影响
		this->upload_data_queue.DequeueToBuffer(upload_data_buf);

		if (!this->upload_data_buf.empty()) {

			int i = 0;
			Json::Value data;
			Json::FastWriter json_writer;

			for (auto element : this->upload_data_buf) {
				data.append(element->GetJSONData());
				if (i++ % this->zhuge_sdk->sdk_config->max_send_size == 0) {
					Json::Value root;
					root["data"] = data;
					BuildFullUploadData(root);
					data = Json::Value();
					std::string json_str = json_writer.write(root);
					this->data_storage->Save(json_str);
				}

				if (element->GetDataType() != ZG_PL) {
					delete element;  // 释放上传数据的内存
				}
			}

			if (!data.empty()) {
				Json::Value root;
				root["data"] = data;
				BuildFullUploadData(root);
				std::string json_str = json_writer.write(root);
				this->data_storage->Save(json_str);
			}

			// 清理缓冲
			this->upload_data_buf.clear();
		}

		// 执行数据上传
		this->TransDataWithAPI();
	}

	void ZhugeSDKTaskProcess::Process()
	{
		static int i = 0;

		while (true) {
			if (this->zhuge_sdk->sdk_config->enable_log) {
				std::clog << "[ZhugeSDK] Upload process running：" << i++ << std::endl;
			}

			this->HandleUploadData();

			if (this->stop_mark.load()) {
				if (!this->upload_data_queue.empty()) {
					this->HandleUploadData();
				}
				this->shutdown_promise.set_value();  // 通知处理线程已经关闭
				if (this->zhuge_sdk->sdk_config->enable_log) {
					std::clog << "[ZhugeSDK] Upload process stopped" << std::endl;
				}
				break;
			}

			std::this_thread::sleep_for(
				std::chrono::milliseconds(this->zhuge_sdk->sdk_config->process_interval_milliseconds));
		}
	}

	void ZhugeSDKTaskProcess::AddUploadDataToQueue(ZhugeSDKUploadData* upload_data)
	{
		this->upload_data_queue.Enqueue(upload_data);
	}

	void ZhugeSDKTaskProcess::Run()
	{
		std::thread t([this]{this->Process(); });
		t.detach();
	}

	void ZhugeSDKTaskProcess::Stop(int timeout)
	{
		this->stop_mark.store(true);
		std::future<void> shutdown_future(this->shutdown_promise.get_future());
		shutdown_future.wait_for(std::chrono::milliseconds(timeout));
	}

	void ZhugeSDKTaskProcess::Stop()
	{
		this->stop_mark.store(true);
		std::future<void> shutdown_future(this->shutdown_promise.get_future());
		shutdown_future.get();
	}

	ZhugeSDKTaskProcess::~ZhugeSDKTaskProcess()
	{
		delete this->data_storage;
	}

	// 默认的全局SDK对象
	ZhugeSDK* zhuge_sdk = nullptr;
	std::mutex zhuge_sdk_init_lock;

	// ZhugeSDK方法实现

	ZhugeSDK::ZhugeSDK(ZhugeSDKConfig* zhuge_sdk_config) :
		sdk_config(zhuge_sdk_config)
	{
		this->user_id = "";
		this->stopped.store(false);
		this->platform_info = nullptr;
		this->session_id.store(0);
	}

	std::string ZhugeSDK::GenDeviceID()
	{
#ifdef _WIN32  // Windows
		HW_PROFILE_INFO HwProfInfo;
		if (!GetCurrentHwProfile(&HwProfInfo))
		{
			throw ZhugeSDKException("获取Windows硬件ID信息失败！");
		}
		WCHAR* uuid = HwProfInfo.szHwProfileGuid;
		std::string uuid_str;
		uuid++;
		while (*(uuid) != '}') {
			uuid_str += static_cast<char>(*(uuid));
			uuid++;
		}
		return uuid_str;
#elif __APPLE__  // MacOS
		io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(
			kIOMasterPortDefault, "IOService:/");
		CFStringRef uuidCf = (CFStringRef)IORegistryEntryCreateCFProperty(
			ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
		IOObjectRelease(ioRegistryRoot);
		char buf[512];
		CFStringGetCString(uuidCf, buf, sizeof(buf), kCFStringEncodingMacRoman);
		CFRelease(uuidCf);
		return buf;
#elif __linux__  // Linux
		std::ifstream infile;
		infile.open("/etc/machine-id");  // 读取machine-id
		if (!infile) {
			throw ZhugeSDLException("获取Linux硬件ID信息失败，无法读取文件/etc/machine-id");
		}
		char c;
		std::string machine_id = "";
		while (infile >> c >> std::ws) {
			machine_id += c;
		}
		infile.close();
		return machine_id;
#endif
	}

	void ZhugeSDK::StartProcess()
	{
		// 如果用户没有显式指定设备ID，则根据平台自动计算设备ID
		if (this->sdk_config->user_device_id.empty()) {
			if (this->sdk_config->enable_log) {
				std::clog << "[ZhugeSDK] Use SDK device ID" << std::endl;
			}
			this->auto_device_id = GenDeviceID();
			if (this->sdk_config->enable_log) {
				std::clog << "[ZhugeSDK] SDK generated device ID: " << auto_device_id << std::endl;
			}
		}
		this->upload_process = new ZhugeSDKTaskProcess(this);
		upload_process->Run();
	}

	const std::string& ZhugeSDK::GetDeviceID()
	{
		if (this->sdk_config->user_device_id.empty()) {
			return this->auto_device_id;
		}
		return this->sdk_config->user_device_id;
	}

	void ZhugeSDK::SetCommonEventSystemProperties(Json::Value& common_system_properties)
	{
		if (common_system_properties.empty()) {
			return;
		}

		auto members = common_system_properties.getMemberNames();
		for (auto member : members) {
			this->common_system_properties[member] = common_system_properties[member];
		}

		if (this->sdk_config->enable_log) {
			std::clog
				<< "[ZhugeSDK] Common sys event properties setted: "
				<< this->common_custom_properties.toStyledString()
				<< std::endl;
		}
	}

	void ZhugeSDK::SetCommonEventCustomProperties(Json::Value& common_custom_properties)
	{
		if (common_custom_properties.empty()) {
			return;
		}

		auto members = common_custom_properties.getMemberNames();
		for (auto member : members) {
			this->common_custom_properties[member] = common_custom_properties[member];
		}
		if (this->sdk_config->enable_log) {
			std::clog
				<< "[ZhugeSDK] Common cus event properties setted: "
				<< this->common_custom_properties.toStyledString()
				<< std::endl;
		}
	}

	void ZhugeSDK::Identify(ZhugeUser* user_ptr)
	{
		if (this->stopped.load()) {
			return;
		}

		user_ptr->AddSystemPropertyIfAbsent("tz", this->sdk_config->time_zone); // 完善时区属性
		if (!user_ptr->HasSystemProperty("ct")) {
			using namespace std::chrono;
			const long long ts = duration_cast<milliseconds>(
				system_clock::now().time_since_epoch()).count();
			user_ptr->AddSystemProperty("ct", ts);
		}
		this->user_id = user_ptr->GetUserId();
		this->upload_process->AddUploadDataToQueue(user_ptr);
	}

	void ZhugeSDK::CleanUserId()
	{
		this->user_id = "";
	}

	// 获取系统当前语言信息
	std::string GetSystemLanguage()
	{
#ifdef _WIN32
		LCID locale_id = GetUserDefaultLCID();
		unsigned short lang = locale_id & 0xFF;
		switch (lang) {
		case LANG_CHINESE:
			return "zh";
		case LANG_ENGLISH:
			return "en";
		default:
			return "unknown";
		}
#else
		return "zh";
#endif
	}

	// 获取当前操作系统
	std::string GetOSName()
	{
#ifdef _WIN32
		return "Windows";
#elif __linux__
		return "Linux";
#elif __MAC__
		return "MacOS";
#else
		return "";
#endif
	}

	// 获取当前操作系统版本
	std::string GetOSVersion()
	{
#ifdef _WIN32
		typedef LONG NTSTATUS, *PNTSTATUS;
		typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
		HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
		if (hMod) {
			RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
			if (fxPtr != nullptr) {
				RTL_OSVERSIONINFOW rovi = { 0 };
				rovi.dwOSVersionInfoSize = sizeof(rovi);
				if (0x00000000 == fxPtr(&rovi)) {
					return std::to_string(rovi.dwMajorVersion);
				}
			}
		}
		return "";
#elif __linux__
		return "";
#elif __MAC__
		return "";
#else
		return "";
#endif
	}

	// 获取系统当前分辨率
	std::string GetScreenResolving()
	{
#ifdef _WIN32
		long n_screen_pixel_width = 0, n_screen_pixel_height = 0;
		DEVMODE dm;
		dm.dmSize = sizeof(DEVMODE);
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm);
		n_screen_pixel_width = dm.dmPelsWidth;
		n_screen_pixel_height = dm.dmPelsHeight;
		char buf[127];
		snprintf(buf, sizeof(buf), "%dx%d", n_screen_pixel_width, n_screen_pixel_height);
		return buf;
#else
		return "";
#endif
	}

	ZhugePlatform* ZhugeSDK::GetPlatformInfo()
	{
		ZhugePlatform* platform_ptr = new ZhugePlatform();

		const std::string lang = GetSystemLanguage();
		if (!lang.empty()) {
			platform_ptr->AddSystemProperty("lang", GetSystemLanguage()); // 语言
		}

		const std::string rs = GetScreenResolving();
		if (!rs.empty()) {
			platform_ptr->AddSystemProperty("rs", rs);  // 分辨率
		}

		const std::string os = GetOSName();
		if (!os.empty()) {
			platform_ptr->AddSystemProperty("os", GetOSName());   // OS
		}

		const std::string os_version = GetOSVersion();
		if (!os_version.empty()) {
			platform_ptr->AddSystemProperty("ov", os_version);
		}

		return platform_ptr;
	}

	void ZhugeSDK::Platform(ZhugePlatform* platform_ptr)
	{
		if (this->stopped.load()) {
			return;
		}

		if (!this->user_id.empty()) {
			platform_ptr->AddSystemPropertyIfAbsent("cuid", this->user_id);  // 设置$cuid
		}

		platform_ptr->AddSystemPropertyIfAbsent("tz", this->sdk_config->time_zone); // 完善时区属性

		if (!platform_ptr->HasSystemProperty("ct")) {
			using namespace std::chrono;
			const long long ts = duration_cast<milliseconds>(
				system_clock::now().time_since_epoch()).count();
			platform_ptr->AddSystemProperty("ct", ts);  // 加入当前时间戳
		}
		ZhugePlatform *old_platform = this->platform_info;
		this->platform_info = platform_ptr;
		delete old_platform;
		this->upload_process->AddUploadDataToQueue(platform_ptr);
	}

	void ZhugeSDK::StartSession()
	{
		if (this->stopped.load()) {
			return;
		}

		std::unique_lock<std::mutex> lock(this->session_mutex);

		using namespace std::chrono;
		this->session_id.store(duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()).count());
		ZhugeSessionStart* session_start = new ZhugeSessionStart(this->session_id.load());
		if (!this->user_id.empty()) {  // 设置$cuid
			session_start->AddSystemProperty("cuid", this->user_id);
		}
		session_start->AddSystemProperty("tz", this->sdk_config->time_zone);  // 设置时区
		session_start->AddSystemProperty("ct", session_id.load());  // 事件时间与会话ID一致

		// 操作系统信息
		if (!session_start->HasSystemProperty("os") && this->platform_info != nullptr) {
			session_start->AddSystemProperty("os", platform_info->GetSystemStringProperty("os", "unknown"));
		}
		if (!session_start->HasSystemProperty("ov") && this->platform_info != nullptr) {
			session_start->AddSystemProperty("ov", platform_info->GetSystemStringProperty("ov", "unknown"));
		}

		this->upload_process->AddUploadDataToQueue(session_start);
	}

	void ZhugeSDK::StopSession()
	{
		if (this->stopped.load()) {
			return;
		}

		std::unique_lock<std::mutex> lock(this->session_mutex);

		const long long session_id = this->session_id.load();
		if (session_id == 0) {  // 当前没有会话
			return;
		}

		using namespace std::chrono;
		const long long now = duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()).count();

		if (now < session_id) {  // 当前会话ID不合法
			return;
		}

		ZhugeSessionEnd* session_end = new ZhugeSessionEnd(this->session_id.load());
		if (!this->user_id.empty()) {  // 设置$cuid
			session_end->AddSystemPropertyIfAbsent("cuid", this->user_id);
		}

		session_end->AddSystemPropertyIfAbsent("tz", this->sdk_config->time_zone);  // 设置时区

		session_end->AddSystemPropertyIfAbsent("ct", session_id);  // 事件时间与会话ID一致

		session_end->AddSystemPropertyIfAbsent("dru", now - session_id);  // 计算会话时长

		this->session_id.store(0);
		this->upload_process->AddUploadDataToQueue(session_end);
	}

	void ZhugeSDK::Track(ZhugeEvent* event_ptr)
	{
		if (this->stopped.load()) {  // SDK已经暂停
			return;
		}

		if (!this->user_id.empty()) {
			event_ptr->AddSystemPropertyIfAbsent("cuid", this->user_id);  // 添加cuid
		}

		const long long session_id = this->session_id.load();
		if (session_id != 0) {
			event_ptr->AddSystemPropertyIfAbsent("sid", session_id);  // 添加会话ID
		}

		event_ptr->AddSystemPropertyIfAbsent("tz", this->sdk_config->time_zone); // 完善时区属性

		if (!event_ptr->HasSystemProperty("ct")) {
			using namespace std::chrono;
			const long long ts = duration_cast<milliseconds>(
				system_clock::now().time_since_epoch()).count();
			event_ptr->AddSystemProperty("ct", ts);  //添加事件时间
		}

		// 获取操作系统信息
		if (!event_ptr->HasSystemProperty("os") && this->platform_info != nullptr) {
			event_ptr->AddSystemProperty("os", platform_info->GetSystemStringProperty("os", "unknown"));
		}
		if (!event_ptr->HasSystemProperty("ov") && this->platform_info != nullptr) {
			event_ptr->AddSystemProperty("ov", platform_info->GetSystemStringProperty("ov", "unknown"));
		}

		this->FillCommonEventProperties(event_ptr);  // 填充公共事件属性

		this->upload_process->AddUploadDataToQueue(event_ptr);
	}

	const TrackTimeHolder ZhugeSDK::StartTrack(ZhugeEvent* event_ptr)
	{
		using namespace std::chrono;
		const long long ts = duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()).count();
		event_ptr->AddSystemPropertyIfAbsent("ct", ts);
		return{ event_ptr, ts };
	}

	void ZhugeSDK::EndTrack(TrackTimeHolder& track_time_holder)
	{
		if (this->stopped.load()) {
			return;
		}

		using namespace std::chrono;
		const long long ts = duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()).count();
		const long long duration = ts - track_time_holder.begin_time;
		ZhugeEvent* event_ptr = track_time_holder.event_ptr;
		event_ptr->AddSystemProperty("dru", duration);
		Track(event_ptr);
	}

	void ZhugeSDK::Shutdown(int timeout)
	{
		if (this->sdk_config->enable_log) {
			std::clog << "[ZhugeSDK] shutdown..." << std::endl;
		}
		this->upload_process->Stop(timeout);
	}

	void ZhugeSDK::Shutdown()
	{
		if (this->sdk_config->enable_log) {
			std::clog << "[ZhugeSDK] shutdown..." << std::endl;
		}
		this->upload_process->Stop();
	}

	ZhugeSDK::~ZhugeSDK()
	{
		delete this->upload_process;
		delete this->sdk_config;
	}

	// SDK 配置构造者实现
	ZhugeSDKConfig::ZhugeSDKConfig(
		const std::string api_host, const int api_port, const std::string app_key) :
		api_host(api_host),
		api_port(api_port),
		api_path(DEFAULT_API_PATH),
		app_key(app_key),
		platform(ZHUGE_PLATFORM_JS),
		process_interval_milliseconds(DEFAULT_PROCESS_INTERVAL_MILLISECONDS),
		max_send_size(DEFAULT_MAX_SEND_SIZE),
		enable_log(DEFAULT_ENABLE_LOG),
		enable_debug(DEFAULT_ENABLE_DEBUG),
		user_device_id(""),
		time_zone(DEFAULT_TIME_ZONE),
		api_connection_timeout(DEFAULT_API_CONNECTION_TIMEOUT_S),
		api_read_timeout(DEFAULT_API_READ_TIMEOUT_S),
		api_write_timeout(DEFAULT_API_WRITE_TIMEOUT_S),
		max_storage_records(DEFAULT_MAX_STORAGE_RECORDS),
		storage_file_path(""){};

	ZhugeSDKConfig& ZhugeSDKConfig::APIPath(std::string api_path)
	{
		this->api_path = api_path;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::Platform(std::string platform)
	{
		this->platform = platform;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::ProcessIntervalMilliseconds(const int process_interval_milliseconds)
	{
		this->process_interval_milliseconds = process_interval_milliseconds;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::MaxSendSize(const int max_send_size)
	{
		this->max_send_size = max_send_size;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::EnableLog(const bool enable_log)
	{
		this->enable_log = enable_log;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::EnableDebug(const bool enable_debug)
	{
		this->enable_debug = enable_debug;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::UserDeviceID(const std::string user_device_id)
	{
		this->user_device_id = user_device_id;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::TimeZone(int time_zone)
	{
		this->time_zone = time_zone;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::APIConnectionTimeout(const int api_connection_timeout)
	{
		this->api_connection_timeout = api_connection_timeout;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::APIReadTimeout(const int api_read_timeout)
	{
		this->api_read_timeout = api_read_timeout;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::APIWriteTimeout(const int api_write_timeout)
	{
		this->api_write_timeout = api_write_timeout;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::MaxStorageRecords(const unsigned int max_storage_records)
	{
		this->max_storage_records = max_storage_records;
		return *this;
	}

	ZhugeSDKConfig& ZhugeSDKConfig::StorageFilePath(const std::string storage_file_path)
	{
		this->storage_file_path = storage_file_path;
		if (!storage_file_path.empty()) {  // 非空字符串
			#ifdef _WIN32
			#else
			if (storage_file_path.at(storage_file_path.size() - 1) != '/') {
				this->storage_file_path += "/";
			}
			const std::string cmd = "mkdir -p " + this->storage_file_path;
			system(cmd.c_str());  // ensure the data dir existed
			#endif
		}
		return *this;
	}

	std::ostream& operator<<(std::ostream& out, ZhugeSDKConfig config)
	{
		return out << "[api_host = " << config.api_host
			<< ", api_port = " << config.api_port
			<< ", api_path = " << config.api_path
			<< ", app_key = " << config.app_key
			<< ", platform = " << config.platform
			<< ", process_interval_milliseconds = " << config.process_interval_milliseconds
			<< ", max_send_size = " << config.max_send_size
			<< ", enable_log = " << config.enable_log
			<< ", enable_debug = " << config.enable_debug
			<< ", user_device_id = " << config.user_device_id
			<< ", time_zone = " << config.time_zone
			<< ", api_connection_timeout = " << config.api_connection_timeout
			<< ", api_read_timeout = " << config.api_read_timeout
			<< ", api_write_timeout = " << config.api_write_timeout
			<< ", max_storage_records = " << config.max_send_size
			<< "]";
	}

	// SDK相关函数实现
	void init_zhuge_sdk(
		const std::string& api_host, const int api_port, const std::string& app_key)
	{
		std::unique_lock<std::mutex> lock(zhuge_sdk_init_lock);
		if (zhuge_sdk != nullptr) {
			throw ZhugeSDKException("zhuge_sdk has already been initialized!");
		}

		ZhugeSDKConfig* sdk_config = new ZhugeSDKConfig(api_host, api_port, app_key);
		zhuge_sdk = new ZhugeSDK(sdk_config);
		if (sdk_config->enable_log) {
			std::clog << "[ZhugeSDK] Init zhuge sdk, config items: " << *sdk_config << std::endl;
		}
		zhuge_sdk->StartProcess();
	}

	void init_zhuge_sdk(ZhugeSDKConfig& zhuge_sdk_config)
	{
		std::unique_lock<std::mutex> lock(zhuge_sdk_init_lock);
		if (zhuge_sdk != nullptr) {
			throw ZhugeSDKException("zhuge_sdk has already been initialized!");
		}
		zhuge_sdk = new ZhugeSDK(new ZhugeSDKConfig(zhuge_sdk_config));
		if (zhuge_sdk_config.enable_log) {
			std::clog << "[ZhugeSDK] Init zhuge sdk, config items: " << zhuge_sdk_config << std::endl;
		}
		zhuge_sdk->StartProcess();
	}

	void shutdown_zhuge_sdk()
	{
		zhuge_sdk->Shutdown();
	}

	void shutdown_zhuge_sdk(int timeout)
	{
		zhuge_sdk->Shutdown(timeout);
	}

#ifdef _WIN32
	std::string GBK_TO_UTF8(const std::string &source)
	{
		enum { GB2312 = 936 };

		unsigned long len = ::MultiByteToWideChar(GB2312, NULL, source.c_str(), -1, NULL, NULL);
		if (len == 0)
			return std::string();
		wchar_t *wide_char_buffer = new wchar_t[len];
		::MultiByteToWideChar(GB2312, NULL, source.c_str(), -1, wide_char_buffer, len);

		len = ::WideCharToMultiByte(CP_UTF8, NULL, wide_char_buffer, -1, NULL, NULL, NULL, NULL);
		if (len == 0) {
			delete[] wide_char_buffer;
			return std::string();
		}
		char *multi_byte_buffer = new char[len];
		::WideCharToMultiByte(CP_UTF8, NULL, wide_char_buffer, -1, multi_byte_buffer, len, NULL, NULL);

		std::string dest(multi_byte_buffer);
		delete[] wide_char_buffer;
		delete[] multi_byte_buffer;
		return dest;
	}
#else
	std::string GBK_TO_UTF8(const std::string &source)
	{
		return source;
	}
#endif
}
