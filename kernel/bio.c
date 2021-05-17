// Buffer cache
//
// the buffer cache is a linked list of buf structures holding
// cached copies of disk block contents. caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// interface:
// * to get a buffer for a particular disk block, call bread.
// * after changing buffer data, call bwrite to write it to disk
// * when done with the buffer, call brelease
// * do not use the buffer after calling brelease
// * only one process at a time can use a buffer, so do keep them longer than necessary
//
// the implementation uses two state flags internally
// * B_VALID: the buffer data has been read from the disk
// * B_DIRTY: the buffer data has been modified and needs to be written to disk

