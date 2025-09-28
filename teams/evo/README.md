# EvoAI for MechMania 4: README

## Overview

`EvoAI` is an adaptive artificial intelligence team designed to compete in the MechMania 4 programming contest. It utilizes a hybrid approach, combining a robust tactical State Machine implemented in C++ with an iterative learning mechanism driven by a Genetic Algorithm (GA) implemented in Python.

The core C++ client (`EvoAI.C`/`.h`) manages individual ship behavior. The `ga_optimizer.py` utility runs simulations, evaluates performance, and evolves the parameters governing these behaviors over successive games to maximize the score.

## Project Structure

The project assumes the following structure: