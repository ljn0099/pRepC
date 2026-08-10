# pRepC

pRepC is a lightweight C library for sending sports to PSKReporter.

It is designed to be small and portable, being suitable for desktop and embedded systems.

It currently supports POSIX and Windows through a small platform abstraction layer (HAL).

# Example

You can see an usage example in: [examples/example1.c](examples/example1.c)

# Usage

First we need to understand out two main structs: `prepcReceiverData_t` and `prepcSenderData_t`,
`prepcReceiverData_t` is used to store your data, the receiver of the spots, and `prepcSenderData_t` is used to store
the data of the people that you are receiver from your station, the spots.

This structs are initialized by setting them to 0, ej: `prepcReceiverData_t receiverData = {0};`

And the values are set using setters, that is because we need to also store its len and in the case of the numbers the numbers
of bytes needed to store them, this setters also gives us more comprehensive semantics in more complex values.

Remember that the max length of a string is **254 bytes**, it is encoded as UTF-8.

## Receiver Data

For the `prepcReceiverData_t` the following setters are available:
- `prepcError_t prepc_receiver_data_set_callsign(prepcReceiverData_t *receiverData, const char *callsign, size_t len);`
Used to set the callsign of the receiver station

- `prepc_receiver_data_set_locator(prepcReceiverData_t *receiverData, const char *locator, size_t len);`
Used to set the Maidenhead of the receiver station.

- `prepc_receiver_data_set_decoder_software(prepcReceiverData_t *receiverData, const char *decoderSoftware, size_t len);`
Used to set the name and version of the decoding software.

- `prepcError_t prepc_receiver_data_set_antenna_info(prepcReceiverData_t *receiverData, const char *antennaInfo, size_t len);`
Used to set a free-form description of the receiving antenna.

- `prepcError_t prepc_receiver_data_set_persistent_id(prepcReceiverData_t *receiverData, const char *persistentId, size_t len);`
Used to set a random string that identifies the sender and may be used in the future as a primitive form of security. 

- `prepcError_t prepc_receiver_data_set_rig_info(prepcReceiverData_t *receiverData, const char *rigInfo, size_t len);`
Used to set A description of the Rig in use, preferably include most significant information first so entries can be
grouped automatically

- `void prepc_receiver_data_reset(prepcReceiverData_t *receiverData);`
Used to reset the struct to zero.

From here the mandatory fields are the **callsign**, **locator** and **decoder software**.

This struct must be provided at least once per context by calling `prepc_ctx_set_receiver()` before using any other context functions.

The context only stores a pointer to the `prepcReceiverData_t` it does not copy or take ownership of it. Therefore, the struct itself and all strings/data referenced by it must remain valid for the entire lifetime of the associated `prepcCtx_t`.

If you modify the contents of the struct, you must call `prepc_ctx_set_receiver()` again with the updated struct before
calling any other context functions.

## Sender Data

For the `prepcSenderData_t` the following setters are available:
- `prepcError_t prepc_sender_data_set_callsign(prepcSenderData_t *senderData, const char *callsign, size_t len);`
Used to set the callsign of the spoted station

- `prepcError_t prepc_sender_data_set_locator(prepcSenderData_t *senderData, const char *locator, size_t len);`
Used to set the Maidenhead of the spoted station.

- `prepcError_t prepc_sender_data_set_frequency(prepcSenderData_t *senderData, uint64_t frequency);`
Used to set the frequency in Hertz of the spoted station.

- `prepcError_t prepc_sender_data_set_snr(prepcSenderData_t *senderData, int64_t snr);`
Used to set the signal to noise ratio of the spoted station.

- `prepcError_t prepc_sender_data_set_imd(prepcSenderData_t *senderData, int64_t imd);`
Used to set the intermodulation distortion of the spoted station.

- `prepcError_t prepc_sender_data_set_mode(prepcSenderData_t *senderData, const char *mode, size_t len);`
Used to set the transmission mode of the spoted station using one of the ADIF values for MODE or SUBMODE, you can use the macros
on `pRepCModes.h` for convenience.

- `prepcError_t prepc_sender_data_set_info_src(prepcSenderData_t *senderData, prepcInfoSrc_t infoSrc, bool testTransmission);`
Used to set the source of the record, it can be automatic, from a call log or a manual entry. Also you can indicate if this
spot is a test transmission.

- `prepcError_t prepc_sender_data_set_flow_start_secs(prepcSenderData_t *senderData, uint64_t flowStartSecs);`
Used to set the time in unix seconds of the transmission.

- `prepcError_t prepc_sender_data_set_message_bits(prepcSenderData_t *senderData, const uint8_t *bytes, size_t len);`
Used to set the raw decoded message data.

- `prepcError_t prepc_sender_data_set_delta_time(prepcSenderData_t *senderData, double deltaTime);`
Used to set the time offset of the message received from the start of the transmission period in uS,
as a value in [-3276000, 3276000], with 100uS precision.

- `prepcError_t prepc_sender_data_set_fractional_frequency_8(prepcSenderData_t *senderData, double fractionalFrequency);`
Used to set the fractional part of the audio frequency with 8-bit precision, as a value in [0.0, 1.0).

- `prepcError_t prepc_sender_data_set_fractional_frequency_16(prepcSenderData_t *senderData, double fractionalFrequency);`
Used to set the fractional part of the audio frequency with 16-bit precision, as a value in [0.0, 1.0).

- `void prepc_sender_data_reset(prepcSenderData_t *senderData);`
Used to reset the struct to zero.

From here the mandatory fields are the **callsign**, **mode**, **infoSrc** and **flowStartSecs**.

The library does not copy or take ownership of the provided strings/data.
Therefore, all strings and data must remain valid until `prepc_ctx_add_sender` returns, as the function serializes the data into the context's internal buffer.

After `prepc_ctx_add_sender` returns, the original strings/data can be modified or reused.

`prepc_ctx_add_sender` does not clear or modify the `prepcSenderData_t` structure itself.
If you want to reset it, use `prepc_sender_data_reset`.

---

Now let's see how to use the context functions, there are only 5 functions.

The first one is to initialize the context `prepcError_t prepc_ctx_init(prepcCtx_t **ctx, const char *host, const char *port)`,
here is an example of how to initialize it, simple as that.

```C
prepcCtx_t *ctx;

if (prepc_ctx_init(&ctx, "report.pskreporter.info", "4739") != PREPC_ERR_OK)
    return -1;
```

The next thing you would want to do is set a receiver for this context, so you need to generate one that you like as previously seen
and set it with: `prepcError_t prepc_ctx_set_receiver(prepcCtx_t *ctx, const prepcReceiverData_t *receiverData);`.

The receiver once set will persist during the entire context until you call again the function, you can change the receiver at any
time you want, if there is data in the transmission buffer it will be sent automatically and the new spots you add will be reported
with that new receiver.

Then, to add a spot/sender we can use the `prepcError_t prepc_ctx_add_sender(prepcCtx_t *ctx, const prepcSenderData_t *senderData);`,
this function is very easy to use, when the buffers gets full it will automatically send it and set the new spot to a new buffer
regenerating the receiverData automatically, also you can send senderData with different fields and it the library will manage it
but it will be less efficient because every time the number of fields change the library needs to send the buffer sending out buffer
that are partially full. (Remember that this function does NOT clear the data on senderData)

If you don't want to wait for a buffer to get full to send it, or at the end of the program you can use the function
`prepcError_t prepc_ctx_flush(prepcCtx_t *ctx, uint64_t minIntervalSecs, bool allowReceiverDataOnly);`, if you set the
minIntervalSecs the transmission will be sent, or you can put any other timeout in seconds, if the last buffer was sent later than
the specified timeout the buffer will be sent.

The `allowReceiverDataOnly` can be useful if you want to send a package with only the receiver data.

This function doesn't reset the or erase the receiver data that you set so there is NO need to call again `prepc_ctx_set_receiver`.

Finally to free the context you can use `void prepc_ctx_free(prepcCtx_t *ctx);`, but this function does **NOT** send the transmission
buffer so make sure to call `prepc_ctx_flush` before freeing the context.

## Error handling

As for the setter functions, they can return either `PREPC_ERR_INVALID_ARGS` when you send an invalid pointer/value and will NOT set
that to the data or it can return `PREPC_ERR_OK` on success.

The context functions also leave the context in a consistent state so for example if the network is unreachable and 
`prepc_ctx_set_receiver` returns `PREPC_ERR_NETWORK` you can try to call it again later when the network is available again.

# Notes

The library automatically handles the sequence number overflow, DNS changes and templates limitations by resetting the context
so the server effectively sees it as an other entity.

pRepC currently does not implement the PSKReporter recommendation of limiting how often the same callsign is reported,
but there are plans do module separated from the core for that.
