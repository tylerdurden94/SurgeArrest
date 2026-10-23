#include "apc_delivery.h"
#include <assert.h>
#include <stdio.h>

int main() {
  ApcDelivery command;
  command.queue(1, 1, 100);
  assert(command.pending());
  assert(!command.confirm(1, 1)); // Acceptance alone is never confirmation.
  command.sent(200);
  assert(command.phase == ApcDelivery::Awaiting && command.attempts == 1);
  assert(!command.tick(6199));
  assert(command.tick(6200)); // A lost reply gets a retry and fresh session.
  command.sent(6300);
  assert(command.tick(12300));
  command.sent(12400);
  assert(!command.tick(18400));
  assert(command.phase == ApcDelivery::Unconfirmed && !command.pending());
  assert(command.attempts == 3); // Never actuate indefinitely without evidence.
  assert(!command.tick(100000));

  command.queue(2, 1, 200000);
  command.sent(200100);
  command.queue(3, 0, 200200); // OFF immediately supersedes an unconfirmed ON.
  assert(command.target == 0 && command.attempts == 0);
  assert(!command.confirm(2, 1));
  assert(!command.confirm(3, 0));
  command.sent(200300);
  assert(!command.confirm(2, 0)); // Wrong generation, even with matching value.
  assert(!command.confirm(3, 1)); // Genuine readback of the wrong physical state.
  assert(command.confirm(3, 0));
  assert(command.phase == ApcDelivery::Confirmed && !command.pending());
  assert(!command.tick(300000));

  command.queue(4, 1, 400000);
  assert(!command.tick(444999));
  assert(command.phase == ApcDelivery::Queued);
  command.tick(445000); // A strip that never fetches cannot switch much later.
  assert(command.phase == ApcDelivery::Unconfirmed && command.attempts == 0);

  command.queue(5, 1, 0xfffffff0U);
  command.sent(0xfffffff1U);
  assert(!command.tick(0x00000010U));
  assert(command.tick(0x00001761U)); // millis() wrap preserves retry timing.
  command.tick(0x0000b000U);
  assert(command.phase == ApcDelivery::Unconfirmed);

  ApcDelivery outlets[6];
  for (int i = 0; i < 6; ++i) outlets[i].queue(100 + i, i % 2, 1000);
  outlets[0].sent(1100);
  outlets[1].sent(1200);
  outlets[0].confirm(100, 0);
  assert(outlets[1].pending() && outlets[2].phase == ApcDelivery::Queued);
  puts("PASS: bounded retries, expiry, replacement, correlation, clock wrap, independent outlets");
}
