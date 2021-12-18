// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#ifndef RPI_GPIO_H
#define RPI_GPIO_H

#include <stdint.h>

// For now, everything is initialized as output.
class GPIO {
 public:
  // Available bits that actually have pins.
  static const uint32_t kValidBits;

  GPIO();

  // Initialize before use. Returns 'true' if successful, 'false' otherwise
  // (e.g. due to a permission problem).
  bool Init();

  // Initialize inputs.
  // Returns the bits that are actually set.
  uint32_t InitInputs(uint32_t inputs);

  // Initialize outputs.
  // Returns the bits that are actually set.
  uint32_t InitOutputs(uint32_t outputs);

  // Set the bits that are '1' in the output. Leave the rest untouched.
  inline void SetBits(uint32_t value) {
    gpio_port_[0x1C / sizeof(uint32_t)] = value;
  }

  // Get the bit input status of the bits that are '1' in the mask
  inline uint32_t GetBits(uint32_t mask) {
    return gpio_port_[13] & mask;
  }

  // Clear the bits that are '1' in the output. Leave the rest untouched.
  inline void ClearBits(uint32_t value) {
    gpio_port_[0x28 / sizeof(uint32_t)] = value;
  }

  inline void Write(uint32_t value) {
    // Writing a word is two operations. The IO is actually pretty slow, so
    // this should probably  be unnoticable.
    SetBits(value & output_bits_);
    ClearBits(~value & output_bits_);
  }

  inline void busy_nano_sleep(long nanos) {
    busy_wait_impl_(nanos);
  }

 private:
  uint32_t input_bits_;
  uint32_t output_bits_;
  volatile uint32_t *gpio_port_;
  void (*busy_wait_impl_)(long);
};

#endif  // RPI_GPIO_H
