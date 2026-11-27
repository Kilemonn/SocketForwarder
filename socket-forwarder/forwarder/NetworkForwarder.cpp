#include "NetworkForwarder.h"

#include <uuid/uuid.h>

namespace forwarder
{
    std::string NetworkForwarder::getNewUUID()
    {
        uuid_t uuid{};
        char temp[100];
        uuid_generate(uuid);
        uuid_unparse(uuid, temp);
        return std::string(temp);
    }
}
