failsafe
========

## Synopsis

Failsafe is a project to determine if water is still flowing in a closed water loop for computer cooling. If the water is not flowing it will cut the power to the pump and computers. The computers are cooled with an external radiator and pump which is outside the house. Since these systems are far apart a Arduino monitors the flow to ensure that pumps will not run dry or there is no electric short when there is a water leak or an unknown incident.

Inside the house each computer is split into its own water loop via a manifold. Using flowmeters for each loop an http server display the flow-per-second so that the manifold valves could be adjusted manually. The data is also made available as JSON via rest for an external monitoring software solution running on a seperate Raspberry Pi.

## Code Example

As the project progressed more features got added:
- Arduino_email: Monitors a single water loop and send an email when the circuit breakers is activated.
- Arduino_dual: Monitors multiple water loops, expose data in rest for external monitoring and removed email.
- Arduino_nvram: Monitors multiple water loops and make it possible to service a computer. It stores the last config in nvram, so when it restarts it will remember what loops needs to be monitored.

## Motivation

The computers are on 24/7 and with water cooling things sometimes go wrong. But it was also an opportunity to learn more about hardware programming and the Arduino.

## Installation

Hardware:

- 1x Arduino
- 1x Arduino Ethernetboard (Duemilanova w/Atmega328)
- 3x water flow meter
- 1x 220v switch

## API Reference

- fritzing = logical circuit drawing
- picture = circuit board (With a before picture)

## Tests

There was several leaks over the years and the failsafe works.

## Contributors

You know who you are.....
