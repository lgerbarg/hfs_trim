/*
  Copyright 2009 Louis Gerbarg

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

#include <fcntl.h>

#include "hfs_format.h"

static void *zero_block = NULL;

void trim(int device, uint64_t start, uint64_t end) {
    uint64_t extent_size = end - start;
#if 1
    if (!zero_block) {
        void *zero_block = malloc(128 * 1024);
        bzero(zero_block, 128 * 1024);
    }

    uint64_t current_displacement;

    while (extent_size > 0) {
        uint32_t zero_size = extent_size > 128 * 1024 ? 128 * 1024 : extent_size;
        pwrite(device, zero_block, zero_size, start+current_displacement);
        current_displacement += zero_size;
        extent_size -= zero_size;
    }
#else
    ioctl(device, BLKDISCARD, start, extent_size);
#endif
}

int main(int argc, char **argv) {
   HFSPlusVolumeHeader vh;

   if (argc !=2) {
       printf("%s <raw-device>\n", argv[0]);
       exit(-1);
   }

   //FIXME test to make sure we have a raw device

   int raw_fs = open(argv[1], O_RDWR);
   pread(raw_fs, &vh, sizeof(vh), 1024);
   uint16_t sig = ntohs(vh.signature);
   uint16_t version = ntohs(vh.version);
   uint32_t volumeOffset = 0;
 
   if (sig == kHFSPlusSigWord) {
       printf("HFS+ volume detected\n");
       if(version != kHFSPlusVersion) {
           printf("Unknown version, aborting\n");
           exit(-3);
       }
   } else if (sig == kHFSXSigWord) {
       printf("HFSX volume detected\n");
       if(version != kHFSXVersion) {
           printf("Unknown version, aborting\n");
           exit(-3);
       }
   } else if (sig == kHFSSigWord) {
       printf("HFS volume detected\n");
       HFSMasterDirectoryBlock *mdb = (HFSMasterDirectoryBlock *) (&vh);
       uint16_t embeddedSig = ntohs(mdb->drEmbedSigWord);
       volumeOffset = 512*ntohs(mdb->drAlBlSt) + (ntohs(mdb->drEmbedExtent.startBlock) * ntohl(mdb->drAlBlkSiz));
       printf("L: %lu\n", volumeOffset);
       pread(raw_fs, &vh, sizeof(vh), volumeOffset+1024);
       sig = ntohs(vh.signature);
       version = ntohs(vh.version);

       if (sig == kHFSPlusSigWord) {
           printf("HFS+ volume detected\n");
           if(version != kHFSPlusVersion) {
               printf("Unknown version, aborting\n");
               exit(-3);
           }
       } else if (sig == kHFSXSigWord) {
           printf("HFSX volume detected\n");
           if(version != kHFSXVersion) {
               printf("Unknown version, aborting\n");
               exit(-3);
           }
      } else {
           printf("Could not read embedded volume info, aborting\n");
           exit(-2);
      }
   } else {
       printf("Could not read volume info, aborting");
       exit(-2);
   }

   //FIXME check to make sure the volume was unmounted cleanly, and warn to fsck if not

   uint32_t i;
   uint32_t blockSize = ntohl(vh.blockSize);
   uint32_t totalBlocks = ntohl(vh.totalBlocks);
   uint32_t allocationFileSize = ntohl(vh.allocationFile.totalBlocks);
   uint8_t *allocationFileData = malloc(allocationFileSize * blockSize);

   uint32_t currentBlockDisplacement = 0;

   printf("Block size: %lu\n", blockSize);
   printf("Total blocks: %lu\n", totalBlocks);

   printf("Allocation File Size: %lu\n", allocationFileSize);
   for (i = 0; i < 8; i++) {
       uint32_t startBlock = ntohl(vh.allocationFile.extents[i].startBlock);
       uint32_t blockCount = ntohl(vh.allocationFile.extents[i].blockCount);

       printf("\tExtent %u: %lu at %lu\n", i, startBlock, blockCount);
       ssize_t read_count = pread(raw_fs, allocationFileData + currentBlockDisplacement*blockSize, blockCount*blockSize, volumeOffset + startBlock*blockSize);

       if (read_count == -1) {
           printf("Error (%d) reading allocation file\n", errno);
           exit(-4);
       }
       currentBlockDisplacement += blockCount;
   }

   uint32_t first_unused_block = 0;

   for(i = 0; i < totalBlocks; i++) {
      uint8_t bit = i%8;
      uint8_t byte = allocationFileData[i/8];
      uint8_t used = byte & (1 << (7 - (bit % 8)));

      if (!used) {
          if (!first_unused_block) {
              first_unused_block = i;
          }
      } else {
         if (first_unused_block) {
             //These are HFS blocks, not disk blocks or flash blocks
             printf("TRIM from %lu TO %lu\n", first_unused_block, i-1);
             trim(raw_fs, volumeOffset + first_unused_block*blockSize, volumeOffset + i*blockSize);
        }
        first_unused_block = 0;
     }
   }

   free(allocationFileData);

   close(raw_fs);
   
   return 0;
}
