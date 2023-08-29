#include "Logging.h"

int main(int, char **)
{
    using namespace Common;

    char c = 'd';
    int i = 3;
    unsigned long ul = 65;
    float f = 3.4;
    double d = 34.56;
    const char *s = "test C-string";
    std::string ss = "test string";

    CLogger logger("logging_example.log");

    logger.Log("Logging a char:% an int:% and an unsigned:%\n", c, i, ul);
    logger.Log("Logging a float:% and a double:%\n", f, d);
    logger.Log("Logging a C-string:'%'\n", s);
    logger.Log("Logging a string:'%'\n", ss);

    return 0;
}
