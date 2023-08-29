#include "MemoryPool.h"

struct MyStruct
{
    int data[3];
};

int main(int, char **)
{
    using namespace Common;

    CMemoryPool<double> primitivePool(50);
    CMemoryPool<MyStruct> structPool(50);

    for (auto i = 0; i < 50; ++i)
    {
        auto pPrimitiveRet = primitivePool.Allocate(i);
        auto pStructRet = structPool.Allocate(MyStruct{i, i + 1, i + 2});

        std::cout << "prim elem:" << *pPrimitiveRet << " allocated at:" << pPrimitiveRet << std::endl;
        std::cout << "struct elem:" << pStructRet->data[0] << "," << pStructRet->data[1] << "," << pStructRet->data[2] << " allocated at:" << pStructRet << std::endl;

        if (i % 5 == 0)
        {
            std::cout << "deallocating prim elem:" << *pPrimitiveRet << " from:" << pPrimitiveRet << std::endl;
            std::cout << "deallocating struct elem:" << pStructRet->data[0] << "," << pStructRet->data[1] << "," << pStructRet->data[2] << " from:" << pStructRet << std::endl;

            primitivePool.Deallocate(pPrimitiveRet);
            structPool.Deallocate(pStructRet);
        }
    }

    return 0;
}
