# Flash Translation Layer

referenced: [Understanding Flash: The Flash Translation Layer | flashdba](https://flashdba.com/2014/09/17/understanding-flash-the-flash-translation-layer/)    

When dealing with the flash, we call the "write" operation "program". After one unit is programed, it cannot be reprogramed unless it's "erase"d.   

## Logical Block Mapping
`LBA (Logical Block Mapping)` is like paging, it redirects the block addressing. The block address exposed to the upper layer, is a virtual block address. When an adressing request is delivered to it, it transfer this virtual address to a physical one.    
The virtual address may not be mapped to the same physical address in two addressings.
 
`NAND Flash Translation Layer (NFTL)` is a mechanism used to maximize the lifespan of a NAND flash memory device.   
A `Flash Translation Layer` simply do the following three jobs:   
1.	Write updated information to a new empty page and then divert all subsequent read requests to its new address.
2.	Ensure that newly-programmed pages are evenly distributed across all of the available flash so that it wears evenly.
3.	Keep a list of all the old invalid pages so that at some point later on they can all be recycled ready for reuse.   

With a great algorithm, the FTL may increase the lifespan of a flash memory device.

A `Flash Translation Layer` is comprised of:   
+	Wear leveling algorithms
+	Bad block management
+	Error control algorithms

With the three components mentioned above, a flash translation layer is available to redirect the writes to some blocks, to other blocks, in order to maximize the lifespan of the whole device.     