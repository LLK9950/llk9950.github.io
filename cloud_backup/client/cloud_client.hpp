#pragma once
#include<iostream>
#include<vector>
#include<map>
#include<unordered_map>
#include<fstream>
#include<boost/filesystem.hpp>
#include<sstream>
#include<boost/algorithm/string.hpp>
#include"httplib.h"
class FileUtil
{
public:

	static bool Read(const std::string& name, std::string* body)
	{
		
		std::ifstream ifs(name, std::ios::binary);
		if (ifs.is_open()==false)
		{
			return false;
		}
		int64_t fsize = boost::filesystem::file_size(name);
		body->resize(fsize);
		ifs.read(&(*body)[0], fsize);
		if (ifs.good() == false)
		{
			std::cout << "file " << name << " read data failed\n";
			return false;
		}
		ifs.close();
		return true;
	}
	static bool Write(const std::string& name, std::string& body)
	{
		std::ofstream ofs(name, std::ios::binary);
		if (ofs.is_open() == false)
		{
			return false;
		}
		ofs.write(&body[0], body.size());
		if (ofs.good() == false)
		{
			std::cout << "file" << name << " write  data failed\n";
			return false;
		}
		ofs.close();
		return true;
	}
};


class DataManager
{
public:
	DataManager(const std::string& filename) :_store_file(filename) {}
	bool Inster(const std::string& key, const std::string& val)//插入更新数据
	{
		_backup_list[key] = val;
		Storage();
		return true;
	}
	bool GetEtag(const std::string& key, std::string* val)//通过文件名获取etag信息
	{
		auto it = _backup_list.find(key);
		if (it == _backup_list.end())
		{
			return false;
		}
		*val = it->second;
		return true;
	}
	bool Storage()//持久化存储
	{
		std::stringstream tmp;
		auto it = _backup_list.begin();
		for (; it != _backup_list.end() ; it++)
		{
			tmp << it->first << " " << it->second << "\r\n";
		}
		std::string tmps = tmp.str();//字符流转化为string对象

		FileUtil::Write(_store_file, tmps);
		return true;
	}
	bool InitLoad()//初始化加载
	{
		std::cout << "InitLoad() " << std::endl;
		std::string body;
		if (FileUtil::Read(_store_file, &body) == false)
		{
			
			return false;
		}

		std::vector<std::string> list;
		boost::split(list, body, boost::is_any_of("\r\n"), boost::token_compress_off);
		for (auto it : list)
		{
			size_t pos = it.find(" ");
			if (pos == std::string::npos)
			{
				continue;
			}
			std::string key = it.substr(0, pos - 1);
			std::string val = it.substr(pos + 1);

			Inster(key, val);
		}
		return true;
	}

private:
	std::string _store_file;
	std::unordered_map <std::string, std::string> _backup_list;
};


class CloudeClient//目录监控 -检测需要备份
{
public:
	CloudeClient(const std::string& filename,const std::string &store_file,const std::string ip,uint64_t port)
		:datamanager(store_file),_listen_dir(filename),_srv_ip(ip),_srv_port(port) {}

	bool Start()
	{

		datamanager.InitLoad();

		while (1)
		{

			std::vector<std::string> list;
			GetBackUpFileList(&list);
			for (int i = 0; i < list.size(); i++)
			{
				std::string name = list[i];
				std::string pathname = _listen_dir + name;
				std::cout << pathname << "  need to backup\n";
				std::string body;
				
				//发出请求
				httplib::Client client(_srv_ip, _srv_port);
				std::string req_path = "./" + name;
				std::cout << req_path << std::endl;
				FileUtil::Read(pathname, &body);
				
				auto rsp = client.Put(req_path.c_str(), body, "application/octet-stream");
				if (rsp == nullptr || (rsp != nullptr && rsp->status != 200))
				{//上传失败
					std::cout << pathname << "   backup  failed \n";

					continue;//下次循环重新尝试上传
				}
				std::string etag;
				GetEtag(pathname, &etag);
				datamanager.Inster(name, etag);//备份、更新成功插入信息
				std::cout << pathname << "    backup  successed ! \n";

			}
			Sleep(1000);//1秒检测一次
		}
		return true;
	}


	bool GetBackUpFileList(std::vector<std::string>* list)//获取要文件备份列表
	{
		if (boost::filesystem::exists(_listen_dir)==false)
		{
			boost::filesystem::create_directory(_listen_dir);
		}
		boost::filesystem::directory_iterator begin(_listen_dir);
		boost::filesystem::directory_iterator end;

		for (; begin != end; ++begin)
		{
			if (boost::filesystem::is_directory(begin->status()))
			{//判断是否是文件夹 文件夹不用备份 自动跳过
				continue;
			}
			std::string pathname = begin->path().string();
			std::string name = begin->path().filename().string();
			std::string cur_etag, old_etag;
			GetEtag(pathname, &cur_etag);
			datamanager.GetEtag(name, &old_etag);
			if (cur_etag != old_etag)
			{//文件信息不相等
				list->push_back(name);//需要备份的文件
			}
		}
		return true;
	}
	 bool GetEtag(const std::string& pathname, std::string* etag)//计算文件etag信息
	{
		 int64_t  size = boost::filesystem::file_size(pathname);
		 time_t mtime = boost::filesystem::last_write_time(pathname);
		 *etag = std::to_string(size) + "-" + std::to_string(mtime);
		 return true; 

	}

private:
	
	std::string _listen_dir;//要监控的文件目录名称
	DataManager datamanager;
	std::string _srv_ip;
	uint64_t _srv_port;

};

