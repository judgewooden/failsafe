failsafe
========

## Synopsis

Failsafe is a project to determine if water is still flowing in a closed water loop for computer cooling. If the water is not flowing it will cut the power to the pump and computers. The computers are cooled with an external radioator and pump which is outside the house. Since these systems are far apart and Arduino monitors the flow to ensure that pumps will not run dry or these is no electric shorts when there is a water leak or an unknown incident.

Inside the house I split the water loops with a manifold to each computer. Using flowmeters for each loop, I display the flow-per-second as a http server so that I could manual adjust the valves on the manifold. The data is also available as JSON in a rest API, for an external monitoring software solution running on a seperate Raspberry Pi.

## Code Example

As I progressed the project I have multiple versions of software. Each improving as the project got bigger.
- Arduino_email: This monitors a single water loop and send an email when the circuit breakers is activated.
- Arduino_dual: This monitors multiple water loops.
- Arduino_nvram: This monitors multiple water loops,and make it possible to remove a single computer for service. It stores the last config in nvram, so when it restarts it will remember what loops needs to be monitored in the manifold.

## Motivation

This was a great monivation to learn more about hardware programming and the Arduino. The Arduino is very stable, very fast and the programming language is very advanced. The real reason is that I have my computers on 24/7 and feel better because I have this failsage.

## Installation

Hardware:

1x Arduino
1x Arduino Ethernetboard (Duemilanova w/ Atmega328)
1x water flow meter
1x 220v switch

## API Reference

fritzing = logical circuit drawing
picture = circuit board (With a picture before circuit boards)

## Tests

I had several leaks over the years and the failsafe does work.

## Contributors

You know who you are.....

