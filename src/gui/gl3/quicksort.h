typedef struct
{
    int  index;
    char* value;
} SortItem;

static void swap(SortItem* a, SortItem* b)
{
    SortItem t = *a;
    *a = *b;
    *b = t;
}

static int partition(SortItem e[],int low, int high)
{
    char* pivot = e[high].value;

    int i = low -1;

    for(int j = low; j <= high - 1;++j)
    {
        if(strcmp(e[j].value,pivot) <= 0)
        {
            ++i;
            swap(&e[i],&e[j]);
        }
    }

    swap(&e[i+1],&e[high]);
    return i + 1;
}

static void quicksort(SortItem items[], int low, int high)
{
    if(low < high)
    {
        int pi = partition(items,low,high);

		quicksort(items, low, pi - 1);
		quicksort(items, pi + 1, high);
    }

}
