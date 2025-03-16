#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

typedef struct MemoryBlock {
    int start;        // start address
    int size;         // block size
    int is_free;      // 1 for free, 0 for allocated
    char PID[10];     // process ID (empty if free)
    struct MemoryBlock *next; // pointer to the next block as a linkedlist
    struct MemoryBlock *prev; // pointer to the previous block as a linkedlist
} MemoryBlock;

MemoryBlock *head = NULL; 

void initializeMemory(int size) {
    head = (MemoryBlock *)malloc(sizeof(MemoryBlock));
    head->start = 0;
    head->size = size;
    head->is_free = 1;
    strcpy(head->PID, "");
    head->next = NULL;
    head->prev = NULL;
}

void printError(char *error){

    fprintf(stderr, "%s\n", error);
}


void Allocate(char *PID, int size, char *type) {
    MemoryBlock *current = head;
    MemoryBlock *best_block = NULL;
    

    // find a block according to input
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (type[0] == 'F') { // first Fit
                best_block = current;
                break;
            } else if (type[0] == 'B') { // best Fit
                if (best_block == NULL || current->size < best_block->size) {
                    best_block = current;
                }
            } else if (type[0] == 'W') { // worst Fit
                if (best_block == NULL || current->size > best_block->size) {
                    best_block = current;
                }
            }
        }

        current = current->next;
    }

    if (best_block == NULL) {
        printError("ERROR: Not enough memory available.");
        return;
    }

    if (best_block->size == size) {
        
        best_block->is_free = 0;
        strcpy(best_block->PID, PID);
    } else {
        // split the block
        MemoryBlock *new_block = (MemoryBlock *)malloc(sizeof(MemoryBlock));
        new_block->start = best_block->start; //0
        new_block->size = size;             
        new_block->is_free = 0;
        strcpy(new_block->PID, PID);

        // update the remaining free block
        best_block->start += size;
        best_block->size -= size;

        // insert the new block into the linked list
        new_block->next = best_block;
        new_block->prev = best_block->prev;
        if(new_block->prev == NULL){
            head = new_block;
        }else{
            new_block->prev->next = new_block;
        }
        
        best_block->prev = new_block;

    }

    printf("Allocated %d bytes to process %s.\n", size, PID);
}




void Deallocate(char *PID) {
    MemoryBlock *current = head;
    
    
    
    while (current != NULL) {
        if (!current->is_free && strcmp(current->PID, PID) == 0) {
            // mark the block as free
            current->is_free = 1;
            strcpy(current->PID, "");
            

            // merge with the previous block if it is free
            if (current->prev != NULL && current->prev->is_free) {
                MemoryBlock *prev = current->prev;
                prev->size += current->size;
                prev->next = current->next;
                if (current->next != NULL) {
                    current->next->prev = prev;
                }

                free(current); // Free the current block as it is merged
                current = prev;
            }

            // merge with the next block if it is free
            if (current->next != NULL && current->next->is_free) {
                MemoryBlock *next = current->next;
                current->size += next->size;
                current->next = next->next;
                if (next->next != NULL) {
                    next->next->prev = current;
                }
                free(next); // free the next block (it is merged)
            }

            printf("Deallocated memory from process %s.\n", PID);
            return;
        }

        current = current->next;
    }

    
    printError("ERROR: Process ID not found.");
}


void Status() {
    MemoryBlock *current = head;
    int total_free = 0;
    int total_allocated = 0;

    printf("Memory Status:\n");

    // while loop to traverse through memory blocks
    while (current != NULL) {
        int end_address = current->start + current->size - 1;
        if (current->is_free) {
            printf("Addresses [%d:%d] Unused\n", current->start, end_address);
            total_free += current->size;
        } else {
            printf("Addresses [%d:%d] Process %s\n", current->start, end_address, current->PID);
            total_allocated += current->size;
        }
        current = current->next;
        //printf("%d ,%d", current->start, current->size);
    }

    printf("Total free memory: %d bytes\n", total_free);
    printf("Total allocated memory: %d bytes\n", total_allocated);
}



void Compact() {
    MemoryBlock *current = head;
    int hole_size = 0;
    int count = 0;
    

    printf("Compacting memory...\n");

    // traverse the list and move all allocated blocks to the beginning
    while (current != NULL) {
        if (current->is_free) {

            hole_size += current->size;
            MemoryBlock *prev = NULL;
            MemoryBlock *next = NULL;

            if(current->prev != NULL){
                current->prev->next = current->next;
                prev = current->prev; // setting previous
            }

            else head = current->next;

            if(current->next != NULL){
                current->next->prev = current->prev;
                next = current->next; // setting next
            }
            free(current); // removing the hole
            
            current = next;
            
        }

        else{
            current->start -= hole_size; // updating the address
            current = current->next;
        }        
    }

    // adding the compact hole at the end

    MemoryBlock *compact_hole = (MemoryBlock *)malloc(sizeof(MemoryBlock));
    compact_hole->size = hole_size;
    compact_hole->is_free = 1;
    compact_hole->next = NULL;
    strcpy(compact_hole->PID, "");

    current = head;
    while(current->next != NULL){
        current = current->next;
    }

    compact_hole->start = current->start + current->size;
    compact_hole->prev = current;
    current->next = compact_hole;

    printf("Compacting is successful\n");

}








int main(int argc, char *argv[]) {
	
	/* TODO: fill the line below with your names and ids */
	printf(" Group Name: ahmet-yusuf  \n Student(s) Name: Ahmet Koca, Yusuf Çağan Çelik \n Student(s) ID: 76779, 79730");
    
    // initialize first hole
    if(argc == 2) {
        
		/* TODO */
        int memorySize = atoi(argv[1]);
        initializeMemory(memorySize);

        
		printf("HOLE INITIALIZED AT ADDRESS %d WITH %d BYTES\n",/* TODO*/ head->start, /* TODO*/ head->size);
    }
    else {
        printError("ERROR Invalid number of arguments.\n");
        return 1;
    }
    
    while(1){
        char *input = malloc(sizeof(char) * 100);
        printf("allocator>");
        fgets(input, 100, stdin);
        input[strcspn(input, "\n")] = '\0'; // remove newline from input

        if(input[0] == '\0') { // empty input = do nothing 
            continue;
        }


        char* arguments[3];
        char* token = strtok(input, " ");
        int tokenCount = 0;

        // get all arguments from input
        while(token != NULL){
            arguments[tokenCount] = token;
            token = strtok(NULL, " ");
            tokenCount++;
        }
        
         // Handle RQ (Request)
        if (strcmp(arguments[0], "rq") == 0) {
            if (tokenCount == 4) {
                char *pid = arguments[1];
                int size = atoi(arguments[2]);
                char *type = arguments[3];

                // Validate size and type
                if (size <= 0) {
                    printError("ERROR: Memory size must be a positive integer.");
                } else if (strcmp(type, "F") != 0 && strcmp(type, "B") != 0 && strcmp(type, "W") != 0) {
                    printError("ERROR: Invalid allocation strategy. Use 'F', 'B', or 'W'.");
                } else {
                    Allocate(pid, size, type);
                }
            } 

            else {
                printError("ERROR Expected expression: RQ \"PID\" \"Bytes\" \"Algorithm\".");
            }
            
        }
        // RL (Release Memory / Deallocate): Needs 2 arguments and must check if they are valid arguments
        else if(strcmp(arguments[0], "rl") == 0){
            if(tokenCount == 2){
                char *pid = arguments[1];
                Deallocate(pid);
            }
            else{
                printError("ERROR Expected expression: RL \"PID\".");
            }
        }
        // STATUS: Needs 1 argument
        else if(strcmp(arguments[0], "status") == 0){
            if(tokenCount==1){
                Status();
            }
            else{
                printError("ERROR Expected expression: STATUS.");
            }
        }
        // C (Compact): Needs 1 argument
        else if(strcmp(arguments[0], "c") == 0){
            if(tokenCount == 1){
				
                Compact();
            
			}
            else{
                printError("ERROR Expected expression: C.");
            }
        }
        // EXIT: Needs 1 argument
        else if(strcmp(arguments[0], "exit") == 0){
            if(tokenCount == 1){
                printf("Exiting program.\n");
                exit(0);
            }
            else{
                printError("ERROR Expected expression: EXIT.");
            }
        }
        // If command is not recognized, print error message and continue
        else{
            printError("ERROR Invalid command.");
        }
    }
}