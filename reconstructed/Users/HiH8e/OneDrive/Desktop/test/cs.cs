using System;

class Program
{
  static void Main()
  {
    Console.WriteLine("Hello, World!");
    Console.Write("Enter your name: ");
    string name = Console.ReadLine();
    Console.WriteLine("Hello, " + name + "!");

    // Test some C# features
    List<int> numbers = new List<int>();
    numbers.Add(1);
    numbers.Add(2);
    numbers.Add(3);

    Console.WriteLine("Numbers in list:");
    foreach (var num in numbers)
    {
      Console.WriteLine(num);
    }

    Console.WriteLine("Math operations:");
    Console.WriteLine("2 + 3 = " + (2 + 3));
    Console.WriteLine("Math.Sqrt(16) = " + Math.Sqrt(16));
    Console.WriteLine("Math.PI = " + Math.PI);
  }
}