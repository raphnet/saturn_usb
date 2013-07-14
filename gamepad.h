#ifndef _gamepad_h__
#define _gamepad_h__

typedef struct {
	int num_reports;

	int reportDescriptorSize;
	void *reportDescriptor; // must be in flash

	int deviceDescriptorSize; // if 0, use default
	void *deviceDescriptor; // must be in flash

	void (*init)(void);
	void (*update)(void);
	char (*changed)(unsigned char report_id);

	/** \return The number of bytes written */
	char (*buildReport)(unsigned char *buf, unsigned char report_id);
} Gamepad;

#endif // _gamepad_h__


