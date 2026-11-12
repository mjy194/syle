DefinitionBlock ("platform.aml", "DSDT", 2, "SYLE  ", "SYLEREDU", 1)
{
    Scope (\_SB)
    {
        Device (CPU0)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, Zero)
            Name (_STA, 0x0F)
        }
        Device (COM1)
        {
            Name (_HID, EisaId ("PNP0501"))
            Name (_UID, Zero)
            Name (_STA, 0x0F)
            Name (_CRS, ResourceTemplate ()
            {
                IO (Decode16, 0x03F8, 0x03F8, 0x01, 0x08)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) {4}
            })
        }
    }
}
