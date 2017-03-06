
#include "adifall.ext"
#include "DownshaQueue.h"

void * downsha_queue_init(int queue_size, int unit_size)
{
	DownshaQueue * queue = NULL;
	DownshaUnit  * unit  = NULL;
	int            i     = 0;

	queue = (DownshaQueue *)kzalloc(sizeof(*queue));
	if (!queue) return NULL;

	if (queue_size <= 0 || unit_size <= 0)
		return NULL;

	InitializeCriticalSection(&queue->unitCS);
	queue->unit_queue = sk_new(NULL);
	sk_zero(queue->unit_queue);
	queue->unit_array = kzalloc(sizeof(DownshaUnit) * queue_size);
	queue->data_array = kzalloc(unit_size * queue_size);
	queue->unit_num   = 0;
	queue->unit_bgn   = 0;
	queue->unit_size  = unit_size;
	queue->queue_size = queue_size;
	queue->drop_count = 0;
	queue->push_pos  = 0;

#ifdef UNIX
	queue->ready_notify = CreateEvent();
#endif
#ifdef WINDOWS
	queue->ready_notify = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif

    for (i = 0; i < queue_size; i++) {
        unit = (DownshaUnit *)((uint8 *)queue->unit_array + i * sizeof(DownshaUnit));
		unit->stamp = 0;
		unit->data  = (uint8 *)queue->data_array + i * unit_size;
		unit->size  = unit_size;
        sk_push(queue->unit_queue, unit);
    }

    return queue;
}

int downsha_queue_clean(void * vqueue)
{
    DownshaQueue * queue = (DownshaQueue *)vqueue;
    DownshaUnit  * unit  = NULL;

    if (!queue) return -1;

	if (queue->ready_notify) {
#ifdef UNIX
		SetEvent(queue->ready_notify, -10);
		DestroyEvent(queue->ready_notify);
#endif
#ifdef WINDOWS
		SetEvent(queue->ready_notify);
		CloseHandle(queue->ready_notify);
#endif
		queue->ready_notify = NULL;
	}

    EnterCriticalSection(&queue->unitCS);
    sk_free(queue->unit_queue);
    LeaveCriticalSection(&queue->unitCS);
    DeleteCriticalSection(&queue->unitCS);

    if (queue->unit_array) {
        kfree(queue->unit_array);
        queue->unit_array = NULL;
    }

	if (queue->data_array) {
		kfree(queue->data_array);
		queue->data_array = NULL;
	}

	kfree(queue);
    return 0;
}

int downsha_queue_push(void * vqueue, void * pbyte, int bytelen, int stamp)
{
	DownshaQueue * queue  = (DownshaQueue *)vqueue;
	DownshaUnit  * unit   = NULL;
	int            cursor = 0;
	int            block  = 0;
	int            read   = 0;
	int            drop   = 0;

	if (!queue) return -1;
	if (!pbyte || bytelen <= 0) return -2;

	EnterCriticalSection(&queue->unitCS);
	while (read < bytelen) {
		if ((queue->unit_num + 1) >= queue->queue_size) {
			queue->drop_count ++;
			drop = 1;
#if 0
#ifdef _DEBUG
			printf("Queue Push: UnitBgn=%d, UnitNum=%d, UnitSize=%d, Drop=%d\n",
				queue->unit_bgn, queue->unit_num, queue->unit_size, queue->drop_count);
#endif
#endif
			break;
		}
		cursor = (queue->unit_bgn + queue->unit_num) % queue->queue_size;
		unit = (DownshaUnit *)sk_value(queue->unit_queue, cursor);
		unit->stamp = stamp;

		block = bytelen - read;
		if (block > (queue->unit_size - queue->push_pos))
			block = (queue->unit_size - queue->push_pos);
		memcpy((uint8 *)unit->data + queue->push_pos, (uint8 *)pbyte + read, block);
		queue->push_pos += block;
		if (queue->push_pos >= queue->unit_size) {
			queue->unit_num ++;
			queue->push_pos = 0;
		}
		read += block;
	}
    LeaveCriticalSection(&queue->unitCS);

#ifdef UNIX
	SetEvent(queue->ready_notify, 100);
#endif
#ifdef WINDOWS
	SetEvent(queue->ready_notify);
#endif

	return (drop > 0) ? -1 : 0;
}

void * downsha_queue_pull(void * vqueue)
{
	DownshaQueue * queue = (DownshaQueue *)vqueue;
	DownshaUnit  * unit  = NULL;
	int            ret   = 0;
	int            count = 0;

	if (!queue) return NULL;

	while (count ++ < 3) {
		EnterCriticalSection(&queue->unitCS);
		if (queue->unit_num > 0) {
			unit = (DownshaUnit *)sk_value(queue->unit_queue, queue->unit_bgn);
			queue->unit_bgn = (queue->unit_bgn + 1) % queue->queue_size;
			queue->unit_num --;
		}
		LeaveCriticalSection(&queue->unitCS);

		if (unit != NULL)
			break;

#ifdef UNIX
		ret = TimedWaitEvent(queue->ready_notify, 0.1);
#endif
#ifdef WINDOWS
		ret = WaitForSingleObject(queue->ready_notify, 100);
#endif
	}

    return unit;
}
