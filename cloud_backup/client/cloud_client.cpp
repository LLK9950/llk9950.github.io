#include"httplib.h"
#include"cloud_client.hpp"

constexpr auto STORE_FILE = "./list.backup";
constexpr auto LISTEN_DIR = "./backup/";
constexpr auto SERVER_IP = "39.98.148.186";
constexpr auto SERVER_PORT = 9000;
int main()
{
	CloudeClient client(LISTEN_DIR, STORE_FILE, SERVER_IP, SERVER_PORT);
	client.Start();
	return 0;

}