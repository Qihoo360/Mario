#include "filename.h"

std::string CurrentFileName(const std::string& dbname)
{
    return dbname + "/CURRENT";
}
