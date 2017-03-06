
#ifndef _DOWNSHA_QUEUE_H_
#define _DOWNSHA_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct downsha_unit {
		int    stamp;   // unit time stamp
		void * data;    // unit data
		int    size;    // unit size
	} DownshaUnit;

	typedef struct downsha_queue {
		CRITICAL_SECTION unitCS;     // unit lock
		STACK          * unit_queue; // unit queue
		void           * unit_array; // unit array
		void           * data_array; // data array
		int              unit_num;   // unit number
		int              unit_bgn;   // unit begin
		int              unit_size;  // unit size
		int              queue_size; // queue size
		int              drop_count; // drop count
		int              push_pos;   // push position
#ifdef UNIX
		EVENT          * ready_notify; // Unix event notifier
#endif
#ifdef WINDOWS
		HANDLE           ready_notify; // Windows event notifier
#endif
	} DownshaQueue;

	void * downsha_queue_init  (int queue_size, int unit_size);
	int    downsha_queue_clean (void * vqueue);
	int    downsha_queue_push  (void * vqueue, void * pbyte, int bytelen, int stamp);
	void * downsha_queue_pull  (void * vqueue);

#ifdef __cplusplus
}
#endif

#endif // _DOWNSHA_QUEUE_H_
