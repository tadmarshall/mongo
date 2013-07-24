#include <mongo/client/dbclient.h>
 
using namespace mongo;
 
int main() {
   std::vector<HostAndPort> hosts;
   hosts.push_back(HostAndPort("localhost"));
 
   DBClientReplicaSet connection("c2", hosts);
   connection.connect();
}
