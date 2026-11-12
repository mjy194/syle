#include "device_bus.h"
void device_bus_init(struct device_bus *bus) { *bus=(struct device_bus){0}; }
bool device_bus_register(struct device_bus *bus, uint64_t base, uint64_t size,
    device_access_fn access, void *context) {
    if(!access || !size || (base&3) || (size&3) || size>UINT64_MAX-base ||
       bus->count>=DEVICE_BUS_REGIONS) return false;
    uint64_t end=base+size;
    for(unsigned i=0;i<bus->count;i++) {
        const struct device_region *r=&bus->regions[i];
        if(base<r->base+r->size && r->base<end) return false;
    }
    bus->regions[bus->count++]=(struct device_region){base,size,access,context};
    return true;
}
enum device_status device_bus_access(struct device_bus *bus, uint64_t address,
    unsigned width, bool write, uint32_t *value) {
    if(!value || width!=4 || (address&3) || address>UINT64_MAX-4 ||
       (address&0xfff)>0xffc) return DEVICE_FAULT;
    for(unsigned i=0;i<bus->count;i++) {
        struct device_region *r=&bus->regions[i];
        if(address>=r->base && address-r->base<r->size) {
            if(r->size-(address-r->base)<4) return DEVICE_FAULT;
            return r->access(r->context,address-r->base,width,write,value);
        }
    }
    return DEVICE_UNSUPPORTED;
}
