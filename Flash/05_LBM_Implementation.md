# The Implementation of Logic Block Mapping
It is very simple.    

First associate every page (sector) with a header. When they are clean, both the page and the header are all 1.   

As time goes, one page is chosen, and its _free/used_ bit in the header is cleared, to mark the sector no longer free.    

Then the virtual block number is written to its heder, and the new data is written to the chosen sector.    

Next, the _pre-valid/valid_ bit in the header is cleared, to mark the sector is ready for reading.   

Finally, the _valid/obsolete_ bit in the header is cleared, to mark that it is stale.   

The progress above maintains the atomicity of the logical block mapping. No matter at which step the power supply is cut off, the data won't be lost. The worst case is that the power is cut off between the clear of the _free/used_ bit of the new one and the _valid/obsolete_ bit the old one, there will be two physical pages mapped to one virtual page. And the implementation is free to choose one between them, and mark another obsolete. It may discard the update, but it doesn't lose data. And, the implementation may add version infos into the header so it can always choose the newer one.   

It is possible to combine some of the steps and data above. For example, virtual block number may serve as the _free/used_ bit and _valid/obsolete_ bit since it can be set to 3 states: all-1, the virtual address, all-0.    

For logical block mapping, there can be two tables: direct mapping, maps the logical one to the physical one; and a inverse map, does things reversely.   

It is possible to set a limit to each physical page (sector), allowing them only to map one of a specific range of logical pages (like NUMA). This increases the searching (if we do not store direct map, only search the inverse map sequencially), but reduces the flexibility of the mapping and the endurance of the device.     

It is said that the block-mapping is of two stages: virtual block => logical block, logical block => physical block. Don't know the reason for this.   

