/* 
 * File:   ComplianceModes.h
 * Author: W0017688
 *
 * Created on September 17, 2015, 9:31 AM
 */

#ifndef ALTERNATEMODES_H
#define	ALTERNATEMODES_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "TypeC.h"
#include "platform.h"
#include "Log.h"

//#define COMPLIANCE_MODES                // Currently unused
#define tAlternateDRPSwap 40 * 10	// DRP Swap every 40ms

	extern DeviceReg_t Registers;	// Variable holding the current status of the device registers
	extern BOOL USBPDActive;	// Variable to indicate whether the USB PD state machine is active or not
	extern BOOL USBPDEnabled;	// Variable to indicate whether USB PD is enabled (by the host)
	extern UINT32 PRSwapTimer;	// Timer used to bail out of a PR_Swap from the Type-C side if necessary
	extern BOOL IsHardReset;	// Variable indicating that a Hard Reset is occurring
	extern SourceOrSink sourceOrSink;	// Are we currently a source or a sink?

	extern USBTypeCPort PortType;	// Variable indicating which type of port we are implementing
	extern BOOL blnCCPinIsCC1;	// Flag to indicate if the CC1 pin has been detected as the CC pin
	extern BOOL blnCCPinIsCC2;	// Flag to indicate if the CC2 pin has been detected as the CC pin
	extern BOOL blnSMEnabled;	// Flag to indicate whether the TypeC state machine is enabled
	extern ConnectionState ConnState;	// Variable indicating the current connection state
	extern StateLog TypeCStateLog;	// Log for tracking state transitions and times
	extern volatile UINT16 Timer_S;	// Tracks seconds elapsed for log timestamp
	extern volatile UINT16 Timer_tms;	// Tracks tenths of milliseconds elapsed for log timestamp

	extern BOOL blnAccSupport;	// Flag to indicate whether the port supports accessories
	extern UINT16 StateTimer;	// Timer used to validate proceeding to next state
	extern UINT16 PDDebounce;	// Timer used for first level debouncing
	extern UINT16 CCDebounce;	// Timer used for second level debouncing
	extern UINT16 ToggleTimer;	// Timer used for CC swapping in the device
	extern UINT16 DRPToggleTimer;	// Timer used for swapping from UnattachedSrc and UnattachedSnk
	extern UINT16 OverPDDebounce;	//Timer used to ignore traffic less than tPDDebounce
	extern CCTermType CC1TermPrevious;	// Active CC1 termination value
	extern CCTermType CC2TermPrevious;	// Active CC2 termination value
	extern CCTermType CC1TermCCDebounce;	// Debounced CC1 termination value
	extern CCTermType CC2TermCCDebounce;	// Debounced CC2 termination value
	extern CCTermType CC1TermPDDebounce;
	extern CCTermType CC2TermPDDebounce;
	extern CCTermType CC1TermPDDebouncePrevious;
	extern CCTermType CC2TermPDDebouncePrevious;
	extern USBTypeCCurrent SinkCurrent;	// Variable to indicate the current capability we have receivedSetStateAlnternateUnattachedSetStateAlnternateUnattached(void);

	void SetStateAlternateUnattached(void);
	void StateMachineAlternateUnattached(void);
	void SetStateAlternateDRP(void);
	void StateMachineAlternateDRP(void);
	void AlternateDRPSwap(void);
	void AlternateDRPSourceSinkSwap(void);
	void SetStateAlternateUnattachedSource(void);
	void StateMachineAlternateUnattachedSource(void);
	void StateMachineAlternateUnattachedSink(void);
	void SetStateAlternateUnattachedSink(void);
	void SetStateAlternateAudioAccessory(void);
	CCTermType AlternateDecodeCCTerminationSource(void);

#ifdef	__cplusplus
}
#endif
#endif				/* COMPLIANCEMODES_H */