# Flash Translation Layer

referenced: [Understanding Flash: The Flash Translation Layer | flashdba](https://flashdba.com/2014/09/17/understanding-flash-the-flash-translation-layer/)    

When dealing with the flash, we call the "write" operation "program". After one unit is programed, it cannot be reprogramed unless it's "erase"d.   
 
`NAND Flash Translation Layer (NFTL)` is a mechanism used to maximize the lifespan of, and accelate the read/write operations on a NAND flash memory device.   
A `Flash Translation Layer` simply do the following three jobs:   
1.	Write updated information to a new empty page and then divert all subsequent read requests to its new address.
2.	Ensure that newly-programmed pages are evenly distributed across all of the available flash so that it wears evenly.
3.	Keep a list of all the old invalid pages so that at some point later on they can all be recycled ready for reuse.   

With a great algorithm, the FTL may increase the lifespan of a flash memory device.

With the three components mentioned above, a flash translation layer is available to redirect the writes to some blocks, to other blocks, in order to maximize the lifespan of the whole device.     

## Logical Block Mapping
`Logical Block Mapping` is like paging, it redirects the block addressing. The block address exposed to the upper layer, is a virtual block address. When an adressing request is delivered to it, it transfer this virtual address to a physical one.    
The virtual address may not be mapped to the same physical address in two addressings.   
In FTL, logical block mapping is mainly used to redirect the writing to a block to another block. Since a NAND block cannot be reprogramed without erasing, and each block can survive only a limited number of PE cycles, this mechanism is helpful to balance the life of a NAND flash device.   

## Wear Levelling
Since a block can only endure a limited number of PE cycles, it is necessary to balance the PE cycles performed on each block, otherwise some of them may ran out of their lives while others still remain healthy. With the assistance of logical block mapping, it is possible to balance the lives of the blocks. This mechanism is named `wear levelling`.   

## Garbage Collection
It is necessary to collect the pages which are on longer required (invalid) but have not yet been erased.   
The smallest unit of writing (programing) is page, while the one of erasing is block. With this problem, it costs a lot to recycle only the invalid pages and keep the others still.   

## Write Amplification
With `wear levelling` and `garbage collection`, it is possible that, when we want to write some data into the flash storage, the storage performes more than one programing (write) and erasing in response to this write request. The actual data written into the storage is more than the upper layer expected. We name this phenomenon `write amplification`.    
![write amplification](pics/write-amplification.jpg)   

A high value indicates that the storage is under a situation of reduced endurance and performance.     
