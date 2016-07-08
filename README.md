# CacheSimulator
Simple MIPS Cache Simulator using LRU replacement algorithm and Write-Back and Write-Allocate policy

Accepts Cache config file as first argument and mem trace as second argument, expected formats:

Cache config (all on separate lines, all as integers with no other characters or formatting):
Number of Lines in each Set
Number of Bytes in each Line
Total number of Bytes in the Cache

Mem trace:
<accessType>:<size>:<hexAddress>

AccessType is a single char, R or W for Read or Write respectively
Size is the size of the reference as an integer must be word aligned and never larger than line size
hexAddress is the hexadecimal memory address, all addresses are 32 bits.
