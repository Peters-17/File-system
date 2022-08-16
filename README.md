Project: File systems

### Introduction
Recently there have been lots of bank robberies. Just a few days ago, the police have identified a possible subject. The subject's motor vehicle contained, among other items, some hard drives (without the laptop). Apparently, the subject had deleted the files without reformatting the drives. Fortunately, the subject had never taken CS 537 before. This means that the subject does not know that most data and indeed most of the file control blocks still reside on the drives.

The police man know that bank robbers usually take pictures of banks that they plan to rob. Thus, the police hire you, a file system expert, to be part of the forensics team attempting to reconstruct the contents of the disks. Each disk will be given to you in the form of a disk image. A disk image is simply a file containing the complete contents and file system structures (For more, see "Ext2 image file" section below).

Part1 Assignment: 1. write a program called runscan that takes two arguments
                  2. an input file that contains the disk image 
                  3. an output directory where your output files will be stored

Part2 Assignment: find out the filenames of those inodes that represent the jpg files.

In summary, in the final output directory, for each jpg file, there should be a file with an inode number as the filename, and another file with the same filename as the actual one..
