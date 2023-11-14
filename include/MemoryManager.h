#pragma once

#include <memory>
#include <cstring>

#include <EEPROM.h>

class MemoryUnit
{
public:
    MemoryUnit(const uint16_t &memory_addr, const uint16_t &size, const uint8_t* p_data = nullptr);
    MemoryUnit(const MemoryUnit &other);
    MemoryUnit& operator=(const MemoryUnit& other);
    ~MemoryUnit();

    uint16_t address;
    uint16_t size;
    std::unique_ptr<uint8_t> data;
};

class MemoryManager
{
public:
    MemoryManager();
    ~MemoryManager();

    void save(const MemoryUnit &param);
    MemoryUnit load(const uint16_t &addr, const uint16_t &size);

private:
    static constexpr uint16_t EEPROM_SIZE = 1024;
};