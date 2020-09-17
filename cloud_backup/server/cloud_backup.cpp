#include <iostream>
#include<thread>
#include"cloud_backup.hpp"

void text_Compress(char *argv[])
{

  _cloud_sys::CompressUtil::UnCompress(argv[1] , argv[2]);
  std::string file =argv[2];
  file+=".txt";
  _cloud_sys::CompressUtil::UnCompress(argv[2] , file.c_str());
}
void data_text()
{

  _cloud_sys::DataManage data_manage("./test.txt");
  data_manage.InitLoad();
  data_manage.Insert("a.txt","a.txt.gz");
  std::vector<std::string>  list;
  // 获取所有信息测试
  data_manage.GetAllName(& list);
  for(auto i :list)
  {
   std:: cout<<i.c_str()<<std::endl;
  }
  list.clear();
   std:: cout<<"---------------------------"<<std::endl;
  data_manage.NonCompressList ( &list);

  for(auto i :list)
  {
   std:: cout<<i.c_str()<<std::endl;
  }
/* 
  _cloud_sys::DataManage data_manage("./test.txt");
  data_manage.Insert("a.txt","a.txt");
  data_manage.Insert("ab.txt","ab.txt.gz");
  data_manage.Insert("v.txt","v.txt");
  data_manage.Insert("c.txt","c.txt.gz");
  data_manage.Insert("d.txt","d.txt");
  data_manage.Storage();
 */ 
}
void m_non_compress()
{
  _cloud_sys::NotHotCompress ncom(GZFILE_DIR,BACK_DIR);
  ncom.Start();
  return;
}

void thr_http_server()
{
  _cloud_sys::Server srv;
  srv.Start();
  return;
}

int main(int argc, char* argv[])
{
  
  if(boost::filesystem::exists(GZFILE_DIR)==false)
  {
    boost::filesystem::create_directory(GZFILE_DIR);
  }
  if(boost::filesystem::exists(BACK_DIR)==false)
  {
    boost::filesystem::create_directory(BACK_DIR);
  }
    std::thread thr_comprss(m_non_compress);// c++11中线程--启动非热点文件压缩模块
    std::thread thr_server(thr_http_server);//网络通信服务端模块
    thr_comprss.join();//等待线程退出
    thr_server.join();
  return 0;
}
