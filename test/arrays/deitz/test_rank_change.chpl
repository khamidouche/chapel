var A: [1..5, 1..5] int = 1..;

var B: [1..5] int;

writeln(A);

B = A[1..5, 1];
writeln(B);

B = A[1, 1..5];
writeln(B);

def foo(B: [] int) {
  writeln(B);
}

foo(A[1, 1..5]);
foo(A[1..5, 1]);
