#include "btree.h"


typedef enum
{
    left = -1,
    right = 1

}   position_t;


typedef struct
{
    bt_node*     node;
    unsigned int index;

}   nodePosition;

//
//  Define the local functions that are not exposed in the API.
//
static void print_single_node ( btree* btree, bt_node* node );

static bt_node *allocate_btree_node ( btree* btree );

static void btree_insert_nonfull ( btree* btree, bt_node* parent_node,
                                   void* data );

static int free_btree_node ( bt_node* node );

static nodePosition get_btree_node ( btree* btree, void* key );

static int delete_key_from_node ( btree* btree, nodePosition* nodePosition );

static bt_node *merge_nodes ( btree* btree, bt_node* n1, void* kv,
                              bt_node* n2 );

static void move_key ( btree* btree, bt_node* node, unsigned int index,
                       position_t pos );

static nodePosition get_max_key_pos ( btree* btree, bt_node* subtree );

static nodePosition get_min_key_pos ( btree* btree, bt_node* subtree );

static bt_node* merge_siblings ( btree* btree, bt_node* parent,
                                 unsigned int index, position_t pos );

static void btree_traverse_node ( btree* btree, bt_node* subtree,
                                  traverseFunc traverseFunction );

/**
*   Used to create a btree with just an empty root node.  Note that the
*   "order" parameter below is the minumum number of keys that exist in each
*   node of the tree.  This is typically 1/2 of a "full" node.  Also note that
*   the number of keys is one less than the number of child pointers.  That's
*   because each key has a left and a right pointer but two adjacent keys
*   share a pointer (the right pointer of one is the left pointer of the
*   other).
*
*   @param order The order of the B-tree
*   @return The pointer to an empty B-tree
*/
btree *btree_create ( unsigned int order,
                      compareFunc  compareFunction,
                      printFunc    printFunction )
{
    btree* btree;

    //
    //  Go allocate a memory block for a new btree data structure.
    //
    btree = MEM_ALLOC ( sizeof(struct btree) );

    //
    //  Initialize all the fields in the new btree structure.
    //
    btree->order           = order;
    btree->nodeFullSize    = 2 * order - 1;
    btree->sizeofKeys      = ( 2 * order - 1 ) * sizeof(void*);
    btree->sizeofPointers  = 2 * order * sizeof(void*);
    btree->count           = 0;
    btree->compareCB       = compareFunction;
    btree->printCB         = printFunction;

    //
    //  Go allocate the root node of the tree.
    //
    btree->root = allocate_btree_node ( btree );

    //
    //  Return the pointer to the btree to the caller.
    //
    return btree;
}

/**
*       Function used to allocate memory for the btree node
*       @param order Order of the B-Tree
*   @param leaf boolean set true for a leaf node
*       @return The allocated B-tree node
*/
static bt_node* allocate_btree_node ( btree* btree )
{
    bt_node* node;

    //
    //  Go create a new node data structure.
    //
    node = (bt_node*)MEM_ALLOC ( sizeof(bt_node) );

    //
    //  Set the number of records in this node to zero.
    //
    node->keysInUse = 0;

    //
    //  Allocate the memory for the array of record pointers and put the
    //  address of that array into the node.
    //
    node->dataRecords = MEM_ALLOC ( btree->sizeofKeys );

    //
    //  Allocate the memory for the array of child node pointers and put the
    //  address of that array into the node.
    //
    node->children = MEM_ALLOC ( btree->sizeofPointers );

    //
    //  Mark this new node as a leaf node.
    //
    node->leaf = true;

    //
    //  Set the tree level to zero.
    //
    node->level = 0;

    //
    //  Set the linked list pointer to NULL.
    //
    node->next = NULL;

    //
    //  Return the address of the new node to the caller.
    //
    return node;
}

/**
*       Function used to free the memory allocated to the b-tree
*       @param node The node to be freed
*       @param order Order of the B-Tree
*       @return The allocated B-tree node
*/
static int free_btree_node ( bt_node* node )
{

    MEM_FREE ( node->children );
    MEM_FREE ( node->dataRecords );
    MEM_FREE ( node );

    return 0;
}

/**
*   Used to split the child node and adjust the parent so that
*   it has two children
*   @param parent Parent Node
*   @param index Index of the parent node where the new record will be put
*   @param child  Full child node
*
*/
static void btree_split_child ( btree* btree,
                                bt_node* parent,
                                unsigned int index,
                                bt_node* child )
{
    int          i = 0;
    unsigned int order = btree->order;

    //
    //  Go create a new tree node as our "new child".
    //
    bt_node* newChild = allocate_btree_node ( btree );

    //
    //  Initialize the fields of the new node appropriately.
    //
    newChild->leaf = child->leaf;
    newChild->level = child->level;
    newChild->keysInUse = btree->order - 1;

    //
    //  Copy the keys from beyond the split point from the old child to the
    //  beginning of the new child.
    //
    for ( i = 0; i < order - 1; i++ )
    {
        newChild->dataRecords[i] = child->dataRecords[i + order];

        //
        //  If this is not a leaf node, also copy the "children" pointers from
        //  the old child to the new one.
        //
        if ( !child->leaf )
        {
            newChild->children[i] = child->children[i + order];
        }
    }
    //
    //  If this is not a leaf node, copy the last node pointer (remember that
    //  there is one extra node pointer for a given number of keys).
    //
    if ( !child->leaf )
    {
        newChild->children[i] = child->children[i + order];
    }
    //
    //  Adjust the record count of the old child to be correct.
    //
    child->keysInUse = order - 1;

    //
    //  Move all the node pointers of the parent up (towards the end) of the
    //  parent node to make room for the new node we just created.
    //
    for ( i = parent->keysInUse + 1; i > index + 1; i-- )
    {
        parent->children[i] = parent->children[i - 1];
    }
    //
    //  Store the new child we just created into the node list of the parent.
    //
    parent->children[index + 1] = newChild;

    //
    //  Move all the keys in the parent one place to the right as well to make
    //  room for the new child.
    //
    for ( i = parent->keysInUse; i > index; i-- )
    {
        parent->dataRecords[i] = parent->dataRecords[i - 1];
    }
    //
    //  Move the key that was used to split the node from the child to the
    //  parent.  Note that it was split at the "index" value specified by the
    //  caller.
    //
    parent->dataRecords[index] = child->dataRecords[order - 1];

    //
    //  Increment the number of records now in the parent.
    //
    parent->keysInUse++;
}

/**
*   Used to insert a key in the non-full node
*   @param btree The btree
*   @param parentNode The node address of the parent node
*   @param the data to be inserted into the tree
*   @return void
*
*   TODO: I can't believe that this function can't fail!  It has no return
*   code!
*
*   TODO: We could make the node search more efficient by implementing a
*   binary search instead of the linear one.
*/

static void btree_insert_nonfull ( btree*   btree,
                                   bt_node* parentNode,
                                   void*    data )
{
    int      i;
    bt_node* child;
    bt_node* node = parentNode;

//
//  Note that his "goto" has been implemented to eliminate a recursive call to
//  this function.
//
insert:

    //
    //  Position us at the end of the records in this node.
    //
    i = node->keysInUse - 1;

    //
    //  If this is a leaf node...
    //
    if ( node->leaf )
    {
        //
        //  Move all the records in the node one position to the right
        //  (working backwards from the end) until we get to the point where
        //  the new data will go.
        //
        while ( i >= 0 &&
                ( btree->compareCB ( data, node->dataRecords[i] ) < 0 ) )
        {
            node->dataRecords[i + 1] = node->dataRecords[i];
            i--;
        }
        //
        //  Put the new data into this node and increment the number of
        //  records in the node.
        //
        node->dataRecords[i + 1] = data;
        node->keysInUse++;
    }
    //
    //  If this is not a leaf node...
    //
    else
    {
        //
        //  Search the node records backwards from the end until we find the
        //  point at which the new data should go.
        //
        while ( i >= 0 &&
                ( btree->compareCB ( data, node->dataRecords[i] ) < 0 ) )
        {
            i--;
        }
        //
        //  Grab the right hand child of this position so we can check to see
        //  if the new data goes into the current node or the right child
        //  node.
        //
        i++;
        child = node->children[i];

        //
        //  If the child node is full...
        //
        if ( child->keysInUse == btree->nodeFullSize )
        {
            //
            //  Go split this node into 2 nodes.
            //
            btree_split_child ( btree, node, i, child );

            //
            //  If the new data is greater than the current record then
            //  increment to the next record.
            //
            if ( btree->compareCB ( data, node->dataRecords[i] ) > 0 )
            {
                i++;
            }
        }
        //
        //  Reset the starting node and go back up and try to insert this
        //  again.  Note that this "goto" actually implements the same thing
        //  as a recursive call except it's more efficient and doesn't blow
        //  out the stack.
        //
        node = node->children[i];
        goto insert;
    }
}

/**
*       Function used to insert node into a B-Tree
*       @param root Root of the B-Tree
*       @param data The address of the record to be inserted
*       @return success or failure
*
*    TODO: This function returns a value but it always returns a 0!
*/
int btree_insert ( btree* btree, void* data )
{
    bt_node* rootNode;

    //
    //  Start the search at the root node.
    //
    rootNode = btree->root;

    //
    //  If the root node is full then we'll need to create a new node that
    //  will become the root and the full root will be split into 2 nodes that
    //  are each approximately half full.
    //
    if ( rootNode->keysInUse == btree->nodeFullSize )
    {
        bt_node* newRoot;

        //
        //  Go create a new node with the same order as the rest of the tree.
        //
        newRoot = allocate_btree_node ( btree );

        //
        //  Increment the level of this new root node to be one more than the
        //  old btree root node.
        //
        newRoot->level = btree->root->level + 1;

        //
        //  Make the new node we just create the root node.
        //
        btree->root = newRoot;

        //
        //  Initialize the rest of the fields in the new root node and make
        //  the old root the first child of the new root node (the left
        //  child).
        //
        newRoot->leaf        = false;
        newRoot->keysInUse   = 0;
        newRoot->children[0] = rootNode;

        //
        //  Go split the old root node into 2 nodes and slosh the data between
        //  them.
        //
        btree_split_child ( btree, newRoot, 0, rootNode );

        //
        //  Go insert the new data we are trying to insert into the new tree
        //  that has the new node as it's root now.
        //
        btree_insert_nonfull ( btree, newRoot, data );
    }
    //
    //  If the root node is not full then just go insert the new data into
    //  this tree somewhere.
    //
    else
    {
        btree_insert_nonfull ( btree, rootNode, data );
    }
    //
    //  Increment the number of records in the btree.
    //
    ++btree->count;

    //
    //  Return the completion code to the caller.
    //
    return 0;
}

/**
*   Used to get the position of the MAX key within the subtree
*   @param btree The btree
*   @param subtree The subtree to be searched
*   @return The node containing the key and position of the key
*/
static nodePosition get_max_key_pos ( btree* btree, bt_node* subtree )
{
    nodePosition nodePosition;
    bt_node* node = subtree;

    //
    //  This loop basically runs down the right hand node pointers in each
    //  node until it gets to the leaf node at which point the value we want
    //  is the last one (right most) in that node.
    //
    while ( true )
    {
        //
        //  If we hit a NULL node pointer, it's time to quit.
        //
        if ( node == NULL )
        {
            break;
        }
        //
        //  Save the current node pointer and the index of the last record in
        //  this node.
        //
        nodePosition.node = node;
        nodePosition.index = node->keysInUse - 1;

        //
        //  If this is a leaf node...
        //
        if ( node->leaf )
        {
            //
            //  If we got to a leaf node, we are at the bottom of the tree so
            //  the value we want is the last record of this node.  We've
            //  already initialized the return structure with this value so
            //  just return the node position data for this node.
            //
            return nodePosition;
        }
        //
        //  If this is not a leaf node then we need to move down the tree to
        //  the right most child in this node.
        //
        else
        {
            //
            //  Make the current node the right most child node of the current
            //  node.
            //
            node = node->children[node->keysInUse];
        }
    }
    //
    //  Return the node position data to the caller.
    //
    return nodePosition;
}

/**
*   Used to get the position of the MIN key within the subtree
*   @param btree The btree
*   @param subtree The subtree to be searched
*   @return The node containing the key and position of the key
*/
static nodePosition get_min_key_pos ( btree* btree, bt_node* subtree )
{
    nodePosition nodePosition;
    bt_node* node = subtree;

    //
    //  This loop basically runs down the left hand node pointers in each node
    //  until it gets to the leaf node at which point the value we want is the
    //  first one (left most) in that node.
    //
    while ( true )
    {
        //
        //  If we hit a NULL node pointer, it's time to quit.
        //
        if ( node == NULL )
        {
            break;
        }
        //
        //  Save the current node pointer and the index of the first record in
        //  this node.
        //
        nodePosition.node = node;
        nodePosition.index = 0;

        //
        //  If this is a leaf node...
        //
        if ( node->leaf )
        {
            //
            //  If we got to a leaf node, we are at the bottom of the tree so
            //  the value we want is the first record of this node.  We've
            //  already initialized the return structure with this value so
            //  just return the node position data for this node.
            //
            return nodePosition;
        }
        //
        //  If this is not a leaf node then we need to move down the tree to
        //  the left most child in this node.
        //
        else
        {
            //
            //  Make the current node the left most child node of the current
            //  node.
            //
            node = node->children[0];
        }
    }
    //
    //  Return the node position data to the caller.
    //
    return nodePosition;
}

/**
*   Merge nodes n1 and n2 (case 3b from Cormen)
*   @param btree The btree
*   @param node The parent node
*   @param index of the child
*   @param pos left or right
*   @return none
*/
static bt_node* merge_siblings ( btree* btree, bt_node* parent,
                                 unsigned int index, position_t pos )
{
    unsigned int i, j;
    bt_node* new_node;
    bt_node* n1, *n2;

    if ( index == ( parent->keysInUse ) )
    {
        index--;
        n1 = parent->children[parent->keysInUse - 1];
        n2 = parent->children[parent->keysInUse];
    }
    else
    {
        n1 = parent->children[index];
        n2 = parent->children[index + 1];
    }

    //Merge the current node with the left node
    new_node = allocate_btree_node ( btree );
    new_node->level = n1->level;
    new_node->leaf = n1->leaf;

    for ( j = 0; j < btree->order - 1; j++ )
    {
        new_node->dataRecords[j] = n1->dataRecords[j];
        new_node->children[j] = n1->children[j];
    }

    new_node->dataRecords[btree->order - 1] = parent->dataRecords[index];
    new_node->children[btree->order - 1] = n1->children[btree->order - 1];

    for ( j = 0; j < btree->order - 1; j++ )
    {
        new_node->dataRecords[j + btree->order] = n2->dataRecords[j];
        new_node->children[j + btree->order] = n2->children[j];
    }
    new_node->children[2*  btree->order - 1] = n2->children[btree->order - 1];

    parent->children[index] = new_node;

    for ( j = index; j < parent->keysInUse; j++ )
    {
        parent->dataRecords[j] = parent->dataRecords[j + 1];
        parent->children[j + 1] = parent->children[j + 2];
    }

    new_node->keysInUse = n1->keysInUse + n2->keysInUse + 1;
    parent->keysInUse--;

    for ( i = parent->keysInUse; i < 2 * btree->order - 1; i++ )
    {
        parent->dataRecords[i] = NULL;
    }

    free_btree_node ( n1 );
    free_btree_node ( n2 );

    if ( parent->keysInUse == 0 && btree->root == parent )
    {
        free_btree_node ( parent );
        btree->root = new_node;
        if ( new_node->level )
            new_node->leaf = false;
        else
            new_node->leaf = true;
    }

    return new_node;
}

/**
*   Move the key from node to another
*   @param btree The B-Tree
*   @param node The parent node
*   @param index of the key to be moved done
*   @param pos the position of the child to receive the key
*   @return none
*/
static void move_key ( btree* btree, bt_node* node, unsigned int index,
                       position_t pos )
{
    bt_node* lchild;
    bt_node* rchild;
    unsigned int i;

    if ( pos == right )
    {
        index--;
    }
    lchild = node->children[index];
    rchild = node->children[index + 1];

    // Move the key from the parent to the left child
    if ( pos == left )
    {
        lchild->dataRecords[lchild->keysInUse] = node->dataRecords[index];
        lchild->children[lchild->keysInUse + 1] = rchild->children[0];
        rchild->children[0] = NULL;
        lchild->keysInUse++;

        node->dataRecords[index] = rchild->dataRecords[0];
        rchild->dataRecords[0] = NULL;

        for ( i = 0; i < rchild->keysInUse - 1; i++ )
        {
            rchild->dataRecords[i] = rchild->dataRecords[i + 1];
            rchild->children[i] = rchild->children[i + 1];
        }
        rchild->children[rchild->keysInUse - 1] =
            rchild->children[rchild->keysInUse];
        rchild->keysInUse--;
    }
    else
    {
        // Move the key from the parent to the right child
        for ( i = rchild->keysInUse; i > 0; i-- )
        {
            rchild->dataRecords[i] = rchild->dataRecords[i - 1];
            rchild->children[i + 1] = rchild->children[i];
        }
        rchild->children[1] = rchild->children[0];
        rchild->children[0] = NULL;

        rchild->dataRecords[0] = node->dataRecords[index];

        rchild->children[0] = lchild->children[lchild->keysInUse];
        lchild->children[lchild->keysInUse] = NULL;

        node->dataRecords[index] = lchild->dataRecords[lchild->keysInUse - 1];
        lchild->dataRecords[lchild->keysInUse - 1] = NULL;

        lchild->keysInUse--;
        rchild->keysInUse++;
    }
}

/**
*   Merge nodes n1 and n2
*   @param n1 First node
*   @param n2 Second node
*   @return combined node
*/
static bt_node *merge_nodes ( btree* btree, bt_node* n1, void* kv,
                              bt_node* n2 )
{
    bt_node* new_node;
    unsigned int i;

    new_node = allocate_btree_node ( btree );
    new_node->leaf = true;

    for ( i = 0; i < n1->keysInUse; i++ )
    {
        new_node->dataRecords[i] = n1->dataRecords[i];
        new_node->children[i] = n1->children[i];
    }
    new_node->children[n1->keysInUse] = n1->children[n1->keysInUse];
    new_node->dataRecords[n1->keysInUse] = kv;

    for ( i = 0; i < n2->keysInUse; i++ )
    {
        new_node->dataRecords[i + n1->keysInUse + 1] = n2->dataRecords[i];
        new_node->children[i + n1->keysInUse + 1] = n2->children[i];
    }
    new_node->children[2*  btree->order - 1] = n2->children[n2->keysInUse];

    new_node->keysInUse = n1->keysInUse + n2->keysInUse + 1;
    new_node->leaf = n1->leaf;
    new_node->level = n1->level;

    free_btree_node ( n1 );
    free_btree_node ( n2 );

    return new_node;
}

/**
*   Used to delete a key from the B-tree node
*   @param btree The btree
*   @param node The node from which the key is to be deleted
*   @param key The key to be deleted
*   @return 0 on success -1 on error
*/

int delete_key_from_node ( btree* btree, nodePosition* nodePosition )
{
    unsigned int maxNumberOfKeys = 2 * btree->order - 1;
    unsigned int i;
    bt_node* node = nodePosition->node;

    //
    //  If this is not a leaf node, return an error code.  This function
    //  should never be called with anything other than a leaf node.
    //
    if ( ! node->leaf )
    {
        return -1;
    }
    //
    //  Move all of the data records down from the position of the data we
    //  want to delete to the end of the node to collapse the data list after
    //  the removal of the target record.
    //
    for ( i = nodePosition->index; i < maxNumberOfKeys - 1; i++ )
    {
        node->dataRecords[i] = node->dataRecords[i + 1];
    }
    //
    //  Decrement the number of keys in use in this node.
    //
    node->keysInUse--;

    //
    //  If the node is now empty, go free it up.
    //
    if ( node->keysInUse == 0 )
    {
        free_btree_node ( node );
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}

/**
*       Function used to delete a node from a  B-Tree
*       @param btree The B-Tree
*       @param key Key of the node to be deleted
*       @param value function to map the key to an unique integer value
*       @param compare Function used to compare the two nodes of the tree
*       @return success or failure
*/

int btree_delete ( btree*   btree,
                   bt_node* subtree,
                   void*    data )
{
    unsigned int i;
    unsigned int index;
    unsigned int splitPoint = btree->order - 1;
    bt_node*     node = NULL;
    bt_node*     rsibling;
    bt_node*     lsibling;
    bt_node*     comb_node;
    bt_node*     parent;
    nodePosition sub_nodePosition;
    nodePosition nodePosition;

    node = subtree;
    parent = NULL;

del_loop:

    while ( true )
    {
        //
        //  If there are no keys in this node, simply return
        //
        if ( !node->keysInUse )
        {
            return -1;
        }
        //
        //  Find the index of the key greater than or equal to the key that we
        //  are looking for.
        //
        i = 0;
        while ( i < node->keysInUse &&
                ( btree->compareCB ( data, node->dataRecords[i] ) > 0 ) )
        {
            i++;
        }
        index = i;

        //
        //  If we found the exact key we are looking for, just break out of
        //  the loop.
        //
        if ( i < node->keysInUse &&
             ( btree->compareCB ( data, node->dataRecords[i] ) == 0 ) )
        {
            break;
        }
        //
        //  If we are in a leaf node and we didn't find the key in question
        //  then we have a "not found" condition.
        //
        if ( node->leaf )
        {
            return -1;
        }
        //
        //  Save the current node as the parent.
        //
        parent = node;

        //
        //  Get the left child of the key that we found.
        //
        node = node->children[i];

        //
        //  If the child pointer is NULL then we have a "not found" condition.
        //
        if ( node == NULL )
        {
            return -1;
        }
        //
        //  If we are at the last populated position in this node...
        //
        if ( index == ( parent->keysInUse ) )
        {
            //
            //  Set the left sibling to the left child of our parent and the
            //  right sibling is NULL.
            //
            lsibling = parent->children[parent->keysInUse - 1];
            rsibling = NULL;
        }
        //
        //  Otherwise, if we are at the beginning of the node, set the left
        //  sibling to NULL and the right sibling to the right child of our
        //  parent.
        //
        else if ( index == 0 )
        {
            lsibling = NULL;
            rsibling = parent->children[1];
        }
        //
        //  Otherwise, we are somewhere in the middle of the node so just set
        //  the left and right siblings to the left and right children of our
        //  parent.
        //
        else
        {
            lsibling = parent->children[i - 1];
            rsibling = parent->children[i + 1];
        }
        //
        //  ??? Not exactly sure what this is looking at but it's comparing
        //  the number of keys in use with the split point for the node...
        //
        //  Not sure what the "t" is that is being referenced below... I think
        //  the "t" is the order of the tree which is also the minumum number
        //  of records in a node (except for the root).
        //
        //  I think this has to do with merging nodes if deleting the target
        //  record from a node would cause it to become smaller than the
        //  minimum size (i.e. the order).
        //
        if ( node->keysInUse == splitPoint && parent )
        {
            //
            //  The current node has (t - 1) keys but the right sibling has >
            //  (t - 1) keys...
            //
            if ( rsibling && ( rsibling->keysInUse > splitPoint ) )
            {
                move_key ( btree, parent, i, left );
            }
            //
            //  The current node has (t - 1) keys but the left sibling has >
            //  (t - 1) keys...
            //
            else if ( lsibling && ( lsibling->keysInUse > splitPoint ) )
            {
                move_key ( btree, parent, i, right );
            }
            //
            //  The left sibling has exactly (t - 1) keys...
            //
            else if ( lsibling && ( lsibling->keysInUse == splitPoint ) )
            {
                node = merge_siblings ( btree, parent, i, left );
            }
            //
            //  The right sibling has exactly (t - 1) keys...
            //
            else if ( rsibling && ( rsibling->keysInUse == splitPoint ) )
            {
                node = merge_siblings ( btree, parent, i, right );
            }
        }
    }
    //
    //  Case 1 : The node containing the key is found and is a leaf node.
    //           The leaf node also has keys greater than the minimum
    //           required so we simply remove the key.
    //
    if ( node->leaf && ( node->keysInUse > splitPoint ) )
    {
        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );
        return 0;
    }
    //
    //  If the leaf node is the root permit deletion even if the number of
    //  keys is less than (t - 1).
    //
    if ( node->leaf && ( node == btree->root ) )
    {
        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );
        return 0;
    }
    //
    //  Case 2: The node containing the key was found and is an internal node.
    //
    if ( ! node->leaf )
    {
        if ( node->children[index]->keysInUse > splitPoint )
        {
            sub_nodePosition =
                get_max_key_pos ( btree, node->children[index] );

            node->dataRecords[index] =
                sub_nodePosition.node->dataRecords[sub_nodePosition.index];

            btree_delete ( btree, node->children[index], data );
            if ( sub_nodePosition.node->leaf == false )
            {
                printf ( "Not leaf\n" );
            }
        }
        else if ( ( node->children[index + 1]->keysInUse > splitPoint ) )
        {
            sub_nodePosition =
                get_min_key_pos ( btree, node->children[index + 1] );

            node->dataRecords[index] =
                sub_nodePosition.node->dataRecords[sub_nodePosition.index];

            btree_delete ( btree, node->children[index + 1], data );
            if ( sub_nodePosition.node->leaf == false )
            {
                printf ( "Not leaf\n" );
            }
        }
        else if ( node->children[index]->keysInUse == splitPoint &&
                  node->children[index + 1]->keysInUse == splitPoint )
        {
            comb_node = merge_nodes ( btree, node->children[index],
                                      node->dataRecords[index],
                                      node->children[index + 1] );

            node->children[index] = comb_node;

            for ( i = index + 1; i < node->keysInUse; i++ )
            {
                node->children[i] = node->children[i + 1];
                node->dataRecords[i - 1] = node->dataRecords[i];
            }
            node->keysInUse--;
            if ( node->keysInUse == 0 && btree->root == node )
            {
                free_btree_node ( node );
                btree->root = comb_node;
            }
            node = comb_node;
            goto del_loop;
        }
    }
    //
    //  Case 3: In this case start from the top of the tree and continue
    //          moving to the leaf node making sure that each node that we
    //          encounter on the way has at least 't' (order of the tree) keys
    //
    if ( node->leaf && ( node->keysInUse > splitPoint ) )
    {
        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );
    }
    //
    //  Decrement the number of records in the btree.
    //
    --btree->count;

    //
    //  All done so return to the caller.
    //
    return 0;
}

/**
*   Function used to get the node containing the given key
*   @param btree The btree to be searched
*   @param key The the key to be searched
*   @return The node and position of the key within the node
*/
nodePosition get_btree_node ( btree* btree, void* key )
{
    nodePosition nodePosition = { 0, 0 };
    bt_node*     node;
    unsigned int i;

    //
    //  Start at the root of the tree...
    //
    node = btree->root;

    //
    //  Repeat forever...
    //
    while ( true )
    {
        //
        //  Initialize our position in this node to the first record of the
        //  node.
        //
        i = 0;

        //
        //  Search the records in this node beginning from the beginning until
        //  we find a record that is equal to or larger than the target...
        //
        while ( i < node->keysInUse &&
                ( btree->compareCB ( key, node->dataRecords[i] ) > 0 ) )
        {
            i++;
        }
        //
        //  If the record that we stopped on matches our target then return it
        //  to the caller.
        //
        if ( i < node->keysInUse &&
             ( btree->compareCB ( key, node->dataRecords[i] ) == 0 ) )
        {
            nodePosition.node  = node;
            nodePosition.index = i;
            return nodePosition;
        }
        //
        //  If the node is a leaf and if we did not find the target in it then
        //  return a NULL position to the caller for a "not found" indicator.
        //
        if ( node->leaf )
        {
            return nodePosition;
        }
        //
        //  Get the left child node and go up and try again in this node.
        //
        node = node->children[i];
    }
    //
    //  This point in the code should never be hit!
    //
    return nodePosition;
}

/**
*       Used to destory btree
*       @param btree The B-tree
*       @return none
*/
void btree_destroy ( btree* btree )
{
    int i = 0;

    bt_node* head;
    bt_node* tail;
    bt_node* child;
    bt_node* del_node;

    //
    //  Start at the root node...
    //
    head = tail = btree->root;

    //
    //  Repeat until the head pointer becomes NULL...
    //
    while ( head != NULL )
    {
        //
        //  If this is not a leaf node...
        //
        if ( ! head->leaf )
        {
            //
            //  For each record in this node...
            //
            //  ??? I think this is chaining all of the nodes into a list
            //  using the "next" pointer in each node before actually deleting
            //  any nodes.  I'm not sure if this is needed - Can't we just
            //  traverse the tree depth first and delete the nodes as we go?
            //
            for ( i = 0; i < head->keysInUse + 1; i++ )
            {
                child = head->children[i];
                tail->next = child;
                tail = child;
                child->next = NULL;
            }
        }
        //
        //  Grab the current list head for the delete that follows and then
        //  update the head to the next node in the list.
        //
        del_node = head;
        head = head->next;

        //
        //  Go delete the node that we saved above.
        //
        free_btree_node ( del_node );
    }
}

/**
*       Function used to search a node in a B-Tree
*       @param btree The B-tree to be searched
*       @param key Key of the node to be search
*       @return The key-value pair
*/
void* btree_search ( btree* btree, void* key )
{
    //
    //  Initialize the return data.
    //
    void* data = NULL;

    //
    //  Go find the given key in the tree...
    //
    nodePosition nodePos = get_btree_node ( btree, key );

    //
    //  If the target data was found in the tree, get the pointer to the data
    //  record to return to the caller.
    //
    if ( nodePos.node )
    {
        data = nodePos.node->dataRecords[nodePos.index];
    }
    //
    //  Return whatever we found to the caller.
    //
    return data;
}

/**
*   Get the max key in the btree
*   @param btree The btree
*   @return The max key
*/
void* btree_get_max ( btree* btree )
{
    nodePosition nodePosition;

    nodePosition = get_max_key_pos ( btree, btree->root );
    if ( nodePosition.node == NULL || nodePosition.index == (unsigned int)-1 )
    {
        return NULL;
    }
    return nodePosition.node->dataRecords[nodePosition.index];
}

/**
*   Get the min key in the btree
*   @param btree The btree
*   @return The min key
*/
void *btree_get_min ( btree* btree )
{
    nodePosition nodePosition;

    nodePosition = get_min_key_pos ( btree, btree->root );
    if ( nodePosition.node == NULL || nodePosition.index == (unsigned int)-1 )
    {
        return NULL;
    }
    return nodePosition.node->dataRecords[nodePosition.index];
}


//
//  This function will traverse the btree supplied and print all of the data
//  records in it in order starting with the minimum record.  This is
//  essentially a "dump" of the contents of the given btree subtree.
//
//  This function will be called by the the "btree_traverse" API function with
//  the root node of the specified btree and then it will call itself
//  recursively to walk the tree.
//
static void btree_traverse_node ( btree* tree, bt_node* subtree,
                                  traverseFunc traverseCB )
{
    int      i;
    bt_node* node = subtree;

    //
    //  If this is not a leaf node...
    //
    if ( !node->leaf )
    {
        //
        //  Loop through all of the sub nodes of the current node and process
        //  each subtree.
        //
        for ( i = 0; i < node->keysInUse; ++i )
        {
            //
            //  Go process all of the nodes in the left subtree of this node.
            //
            btree_traverse_node ( tree, node->children[i], traverseCB );

            //
            //  Once the left subtree has been finished, display the current
            //  data record in this node.
            //
            traverseCB ( node->dataRecords[i] );
        }
        //
        //  Now that we have traversed the left subtree and the current node
        //  entry, go process the right subtree of this record in this node.
        //
        btree_traverse_node ( tree, node->children[i], traverseCB );
    }
    //
    //  Otherwise, if this is a leaf node...
    //
    else
    {
        //
        //  Loop through all of the data records defined in this node and
        //  print them using the print callback function supplied by the user.
        //
        for ( i = 0; i < node->keysInUse; ++i )
        {
            traverseCB ( node->dataRecords[i] );
        }
    }
    return;
}


//
//  This is the user-facing API function that will walk the entire btree
//  supplied by the caller starting with the root node.  As each data record
//  is found in the tree, the user's supplied callback function will be invoked
//  with the pointer to the user data record that was just found.
//
//  The btree given will be walked in order starting with the data record that
//  has the minimum value until the entire tree has been processed.
//
extern void btree_traverse ( btree* tree, traverseFunc traverseCB )
{
    btree_traverse_node ( tree, tree->root, traverseCB );
}


#ifdef DEBUG

/**
*   Used to print the keys of the bt_node
*   @param node The node whose keys are to be printed
*   @return none
*/

static void print_single_node ( btree* btree, bt_node* node )
{

    int i = 0;
    char leader[] = "      ";

    printf ( "\n  Node[%p]\n", node );
    printf ( "    next.......: %p\n", node->next );
    printf ( "    leaf.......: %d\n", node->leaf );
    printf ( "    keysInUse..: %d\n", node->keysInUse );
    printf ( "    level......: %d\n", node->level );
    printf ( "    dataRecords: %p\n", node->dataRecords );
    printf ( "    children...: %p\n", node->children );
    while ( i < node->keysInUse )
    {
        printf ( "      left[%p], right[%p], ",
                 node->children[i], node->children[i+1] );

        btree->printCB ( leader, node->dataRecords[i++] );
    }
}

/**
*       Function used to print the B-tree
*       @param root Root of the B-Tree
*       @param print_key Function used to print the key value
*       @return none
*/

void print_subtree ( btree* btree, bt_node* node )
{
    int i = 0;
    unsigned int current_level;

    bt_node* head, *tail;
    bt_node* child;

    current_level = node->level;
    head = node;
    tail = node;

    printf ( "Btree [%p]\n", btree );
    printf ( "  order.........: %u\n", btree->order );
    printf ( "  fullSize......: %u\n", btree->nodeFullSize );
    printf ( "  sizeofKeys....: %u\n", btree->sizeofKeys );
    printf ( "  sizeofPointers: %u\n", btree->sizeofPointers );
    printf ( "  recordCount   : %u\n", btree->count );
    printf ( "  root..........: %p\n", btree->root );
    printf ( "  compFunc......: %p\n", btree->compareCB );
    printf ( "  printFunc.....: %p\n", btree->printCB );

    while ( head != NULL )
    {
        if ( head->level < current_level )
        {
            current_level = head->level;
        }
        print_single_node ( btree, head );

        if ( ! head->leaf )
        {
            for ( i = 0; i < head->keysInUse + 1; i++ )
            {
                child = head->children[i];
                tail->next = child;
                tail = child;
                child->next = NULL;
            }
        }
        head = head->next;
    }
    printf ( "\n" );
}

#endif
