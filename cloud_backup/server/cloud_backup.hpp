#include <cstdio>
#include<iostream>
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <zlib.h>
#include <pthread.h>
#include "httplib.h"
#include<boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>


#define  NOTHOT_TIME 10 //最后一次访问时间大于10s
#define INTERVALUE_TIME 30 //非热点检测30秒一次
#define BACK_DIR "./backup/"//文件备份路径 
#define GZFILE_DIR "./gzfile/"//文件压缩存放路径
#define DATA_FILE "./list.backup"//数据管理模块的数据备份文件

namespace  _cloud_sys{
	class FileUtil
	{
    public:

		static bool  Read(const std::string &name, std::string *body)//从文件中读取
    {
      std::ifstream  ifs(name,std::ios::binary);
      if(ifs.is_open()==false)
      {
        std::cout<<"file"<<name<<"open is failed READ()函数打开失败\n";
        return false;
      }
      int64_t fsize =boost::filesystem::file_size(name);
      body->resize(fsize);
      ifs.read(&(*body)[0],fsize);
      if(ifs.good()==false)
      {
        std::cout<<"file "<<name<<" read data failed\n";
        return false;
      }
      ifs.close();
      return true;

    }
		static bool  Write(const std::string &name,const  std::string &body)//向文件中写入数据
    {
      std::ofstream ofs(name,std::ios::binary);
      if(ofs.is_open()==false)
      {
        std::cout<<"file"<<name<<" 写 open is failed\n";
        return false;
      }
      ofs.write(&body[0] ,body.size());
      if(ofs.good()==false)
      {
        std::cout<<"file"<<name<<" write  data failed\n";
        return false;
      }
      ofs.close();
      return true;
    }

	};	



	class CompressUtil
	{
    public: 
		
		static bool Compress(const std::string &src, const std::string &dst)//文件压缩 原-压缩包名称
    {
      std::string body;
      FileUtil::Read(src,&body);

      gzFile gf=gzopen(dst.c_str(),"wb");
      if(gf==NULL)
      {
        std::cout<<"open file"<<dst<<" failed\n";
        return false;
      }
      int wlen=0;
      while(wlen<body.size())
      {
        int ret =gzwrite(gf,&body[wlen],body.size()-wlen);
        if(ret==0)
        {
          std::cout<<"file"<<dst<<"write Compress  data failed\n";
          return false;
        }
        wlen+=ret;
      }
      gzclose(gf);
      return true;
    }


		static bool UnCompress(const std::string &src, const std::string &dst)//文件解压缩 压缩包-源文件名称
    {
      std::cout<<"UnCompress()is runing\n ";
      std::ofstream ofs(dst,std::ios::binary);
      if(ofs.is_open()==false)
      {
        std::cout<<"open file"<<dst<<"failed\n";
        return  false;
      }
      gzFile gf = gzopen(src.c_str(),"rb");
      if(gf==NULL)
      {
        std::cout<<"open file"<<src<<"failed\n";
        ofs.close();
        return  false;
      }
      int ret;
      char tmp[4096]={0};
      while((ret=gzread(gf,tmp,4096))>0)
      {
        ofs.write(tmp,ret);
      }
      ofs.close();
      gzclose(gf);
      return true;
    }
	};

	class DataManage
	{
		public:
		DataManage(const std::string &path):_back_file(path)
    {
      pthread_rwlock_init(&_rwlock ,NULL);
    }

		~DataManage()
    {
      pthread_rwlock_destroy(&_rwlock);
    }

		bool Exists(const std::string &name)//文件是否存在
    {
      pthread_rwlock_rdlock(&_rwlock);
      auto it = _file_list.find(name);

      if(it==_file_list.end())
      {
        pthread_rwlock_unlock(&_rwlock);
        return false;
      }
        pthread_rwlock_unlock(&_rwlock);
        return  true;
    }
		bool IsCompress(const std::string &name)//文件是否压缩 文件是否存在
    {
      //文件上传后 文件名称和压缩包名一致
      //文件压缩后，将压缩包名更改为具体包名
      
      pthread_rwlock_rdlock(&_rwlock);
      auto it = _file_list.find(name);

      if(it==_file_list.end())
      {
        pthread_rwlock_unlock(&_rwlock);
        std::cout << "未找到！，致命错误！" << std::endl;
        return false;
      }
      if(it->first==it->second)
      {
        std::cout<<"文件名一致   文件wei压缩\n";
        pthread_rwlock_unlock(&_rwlock);
        return false; //文件名一致 文件未压缩
      }
      std::cout<<"文件已被压缩\n";
      pthread_rwlock_unlock(&_rwlock);
      return true;
    }
		bool NonCompressList(std::vector<std::string> *list)//获取未压缩文件列表
    {
      //遍历_file_list ；将未压缩文件名添加到list中
      //
      pthread_rwlock_rdlock(&_rwlock);
      for(auto it =_file_list.begin();it!=_file_list.end();it++)
      {
        if(it->first==it->second)
        {
          list->push_back(it->first);
        }
      }
        pthread_rwlock_unlock(&_rwlock);
      return true; 
    }
		bool Insert(const std::string &src,const std::string &dst) //插入/更新数据
    {
      pthread_rwlock_wrlock(&_rwlock);//更新修改需要加读写锁
      _file_list[src]=dst;
      pthread_rwlock_unlock(&_rwlock);
      Storage();
      return true;
    }

		bool GetAllName(std::vector<std::string> *list)//获取所有文件名称 向外展示的是文件列表
    {

      pthread_rwlock_rdlock(&_rwlock);
      auto it =_file_list.begin();
      for(;it!=_file_list.end();it++)
      {
        list->push_back(it->first);
      }
      pthread_rwlock_unlock(&_rwlock);
      return  true;
    }

    //根据源文件名称获取压缩包名称
    bool GetGzName(const std::string &src,std::string *dst)
    {
      auto it =_file_list.find(src);
      if(it==_file_list.end())
      {
        return false;
      }
      *dst=it->second;
      return true;
    }
    bool Storage()//数据改变后持久化存储
    {
      //将_file_list中数据存储
      //序列化存储
      // src dst \t\n
      std::stringstream tmp;
      pthread_rwlock_rdlock(&_rwlock);
      auto it =_file_list.begin();
      for(;it!=_file_list.end();it++)
      {
        tmp<<it->first<<" "<<it->second<<"\r\n";
      }
      pthread_rwlock_unlock(&_rwlock);
      FileUtil::Write(_back_file,tmp.str());
      return  true;
    }
		bool InitLoad()//启动时加载原有数据
    {
      //读取数据
      std::string body;

      if(FileUtil::Read(_back_file,&body)==false)
      {
        return false;
      }

      //数据分割
      // boost::split(vector, stc,sep,flag)
      std::vector<std::string> list;
      boost::split(list,body,boost::is_any_of("\r\n"),boost::token_compress_off);
      for(auto i: list)
      {
        size_t pos= i.find(" ");
        if(pos==std::string::npos)
        {
          continue;
        }
        std::string key=i.substr(0,pos);
        std::string value=i.substr(pos+1);
        //将 key val 加到_file_list中
        Insert(key,value);
        std::cout <<"原有数据：" << std::endl;
        std::cout << "key " << key << " " << "value " << value <<std::endl;
      }
        
      return true;

    }
		
    private:
		std::string _back_file;//持久化数据存储文件名称
		std::unordered_map<std::string,std::string> _file_list;//数据管理容器
		pthread_rwlock_t _rwlock;
	};
	

 _cloud_sys:: DataManage  data_manage(DATA_FILE);
 

  class NotHotCompress
  {
    public:
      NotHotCompress(const std::string gz_dir,const std::string bu_dir):_gz_dir(gz_dir),_bu_dir(bu_dir) {}
      bool Start()//循环过程，判断有无热点文件，进行压缩
      {
        //非热点文件 t>n秒
        while(1)
        {
          //获取未压缩列表
          std::vector<std::string> list;
          data_manage.NonCompressList(&list);
          //逐个判断是否是热点文件
          for(int i=0;i<list.size();i++)
          {
            bool ret = FileIsHot(list[i]);
            if(ret==false)
            {
              std::cout<<"-------------------------------------- \n";
              std::cout<<"Non Hot File"<<list[i]<<" \n";
              std::string s_filename=list[i]; //纯源文件名称
              std::string d_filename=list[i]+".gz";//纯压缩包名称
              std::string src_name=_bu_dir +s_filename;//源文件名称
              std::string dst_name=_gz_dir+d_filename;//压缩包名称
          //不是则压缩，并删除源文件
              if(CompressUtil::Compress(src_name,dst_name)==true)
              {
                data_manage.Insert(s_filename,d_filename); //更新数据信息
                unlink(src_name.c_str());//删除源文件
              }
            }
          }
          //休眠
         sleep(INTERVALUE_TIME);
        }
      }
    private:

      bool FileIsHot(const std::string &name)//判断热点文件
      {
        time_t cur_t =time(NULL); //获取当前时间
        struct stat st;
        if(stat(name.c_str(),&st)<0)
        {
          std::cout<<"get file "<<name<<"stat failed\n";
          return false;
        }
        if((cur_t-st.st_atime)>NOTHOT_TIME)
        {
          return false;
        }
        return true;//NOTHOT_TIME内都为热点文件
      }
    private:
      std::string _bu_dir;//压缩前文件所在路径
      std::string _gz_dir;//压缩文件存放路径
  };
    class Server
    {
    public:
      bool Start()
      {
        data_manage.InitLoad();
        _server.Put("/(.*)",UpLoad);
        _server.Get("/list",List);
        _server.Get("/download/(.*)",Download);
        _server.listen("0.0.0.0",9000);
        return  true;

      }
    private:
      static void UpLoad(const httplib::Request &req,httplib::Response &rsp)//文件上传请求
      {
        std::string filename=req.matches[1];
        std::string pathfile=BACK_DIR +filename;
        FileUtil::Write(pathfile,req.body);
        data_manage.Insert(filename,filename);
        rsp.status=200;

        return;
      }

      static void List(const httplib::Request &req,httplib::Response &rsp)//文件列表请求
      {
        std::vector<std::string> list ;
        data_manage.GetAllName(&list);
        std::stringstream tmp;
        tmp<<"<html><body><hr />";
        for(int i=0;i<list.size();i++)
        {
          tmp<<"<a href ='/download/"<<list[i]<<"'>"<<list[i]<<"</a>";
          tmp<<"<hr />";
          //tmp<<"<a href ='/download/a.txt'>a.txt </a>"
        }
        tmp<<"</body></html>";
        rsp.set_content(tmp.str().c_str(),tmp.str().size(),"text/html");
        rsp.status=200;
        return ;
      }



      static void Download(const httplib::Request &req,httplib::Response &rsp)//文件下载请求
      {
        std::string filename =req.matches[1];
        if(data_manage.Exists(filename)==false)//文件找不到
        {
          rsp.status=404;
         return ;
        }
        std::string pathname =BACK_DIR+filename;//源文件所在路径
          std::cout<<pathname<<"源文件所在路径"<<std::endl;
          std::cout<<(data_manage.IsCompress(filename)) << std::endl;;
        if(data_manage.IsCompress(filename)==true)//文件一压缩 ，先解压
        {
        std::cout<<"****************"<<std::endl;
          std::string gzfile;
          data_manage.GetGzName(filename,&gzfile);
          std::string gzpathname =GZFILE_DIR+gzfile;//压缩包路径名
          std::cout<<gzpathname<<"压缩包路径名"<<std::endl;
          CompressUtil::UnCompress(gzpathname,pathname);
          unlink(gzpathname.c_str());
          data_manage.Insert(filename,filename);//更新数据信息 
        }
         //解压好的文件中读取数据到响应客户端
          FileUtil::Read(pathname,&rsp.body);
          rsp.set_header("Content-Type","application/octet-stream");//二进制流下载
          rsp.status=200;
          return;
      }
    private:
      std::string _file_dir;
      httplib::Server _server;
    };
}

