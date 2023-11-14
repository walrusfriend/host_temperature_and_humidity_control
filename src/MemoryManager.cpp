#include "MemoryManager.h"

MemoryUnit::MemoryUnit(const uint16_t &memory_addr, const uint16_t &size, const uint8_t* p_data)
    : address(memory_addr), size(size)
{
    if (p_data == nullptr) {
        data.release();
    }
    data.reset(new uint8_t[size]);
}

MemoryUnit::MemoryUnit(const MemoryUnit &other)
    : address(other.address), size(other.size)
{
    data.reset(new uint8_t[size]);

    std::memcpy(data.get(), other.data.get(), size);
}

MemoryUnit& MemoryUnit::operator=(const MemoryUnit& other) {
    if (this == &other) {
        return *this;
    }

    address = other.address;
    size = other.size;
    data.reset(new uint8_t[size]);

    std::memcpy(data.get(), other.data.get(), size);

    return *this;
}

MemoryUnit::~MemoryUnit() {}

//=====================================================================

MemoryManager::MemoryManager()
{
    if (EEPROM.begin(EEPROM_SIZE) == false)
    {
        Serial.println("EERPOM initialization error!");
    }
}

MemoryManager::~MemoryManager()
{
    EEPROM.end();
}

void MemoryManager::save(const MemoryUnit &param)
{
    for (uint16_t i = 0; i < param.size; ++i)
    {
        EEPROM.write(param.address + i, param.data.get()[i]);
    }

    EEPROM.commit();
}

MemoryUnit MemoryManager::load(const uint16_t &addr,
                               const uint16_t &size)
{
    MemoryUnit param(addr, size);

    for (uint16_t i = 0; i < size; ++i)
    {
        param.data.get()[i] = EEPROM.read(addr + i);
    }

    // Check that we read some data
    bool is_read = false;
    for (uint16_t i = 0; i < size; ++i) {
        if (param.data.get()[i] != 0xFF) {
            is_read = true;
        }
    }

    if (is_read) {
        return param;
    }
    else {
        Serial.printf("INFO: EEPROM has no data in address %d\n", addr);
        return MemoryUnit(addr, size);
    }

}