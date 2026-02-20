#pragma once

namespace nodes {

/**
 * Register math operation nodes:
 * - add, subtract, multiply, divide, modulus
 *
 * All share the same signature:
 *   Inputs:
 *     - csv (Csv, optional): CSV for vector operations
 *     - src (Int|Double|Field): source value
 *     - dest (Field, optional): destination column name
 *     - operand (Int|Double|Field): second operand
 *   Outputs:
 *     - csv (Csv): result CSV (vector mode)
 *     - result (Double): scalar result
 */
void registerMathNodes();

void registerAddNode();
void registerSubtractNode();
void registerMultiplyNode();
void registerDivideNode();
void registerModulusNode();

} // namespace nodes
