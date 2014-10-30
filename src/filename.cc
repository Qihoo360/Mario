#include "filename.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

std::string CurrentFileName(const std::string& dbname)
{
    return dbname + "/CURRENT";
}

std::string NewFileName(const std::string& name, const int current)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s%d", name.c_str(), current);
    return std::string(buf, strlen(buf));
}
