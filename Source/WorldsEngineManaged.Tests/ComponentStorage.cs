using System;
using Xunit;
using WorldsEngine.ECS;

namespace WorldsEngine.Tests
{
    [Component]
    public class TestComponent
    {
        public int Value;
    }

    [Component]
    public class ThinkingComponent : ISimulatedComponent
    {
        public bool HasThought = false;

        public void Simulate()
        {
            HasThought = true;
        }
    }

    public class ComponentStorageTests
    {
        [Fact]
        public void TestAddRemoveHasComponent()
        {
            ComponentStorage<TestComponent> cs = new();
            Entity ent = new(0);
            cs.Set(ent, new TestComponent());
            Assert.True(cs.Contains(ent));
            cs.Remove(ent);
            Assert.False(cs.Contains(ent));
        }

        [Fact]
        public void ThrowsOnNonExistentGet()
        {
            ComponentStorage<TestComponent> cs = new();
            Assert.Throws<ArgumentException>(() => cs.Get(new Entity(0)));
        }

        [Fact]
        public void ThrowsOnNonExistentRemove()
        {
            ComponentStorage<TestComponent> cs = new();
            Assert.Throws<ArgumentException>(() => cs.Remove(new Entity(0)));
        }

        [Fact]
        public void TestDataRetrieval()
        {
            ComponentStorage<TestComponent> cs = new();
            cs.Set(new Entity(0), new TestComponent() { Value = 5 });
            var tc = cs.Get(new Entity(0));
            Assert.Equal(5, tc.Value);
        }

        [Fact]
        public void ThinkingComponent()
        {
            ComponentStorage<ThinkingComponent> cs = new();
            cs.Set(new Entity(), new ThinkingComponent());
            cs.RunSimulate();
            Assert.True(cs.Get(new Entity()).HasThought);
        }

        [Fact]
        public void TestView()
        {
            ComponentStorage<TestComponent> cs = new();
            cs.Set(new Entity(0), new TestComponent() { Value = 5 });
            cs.Set(new Entity(1), new TestComponent() { Value = 6 });

            int numFound = 0;
            foreach (var (ent, comp) in new View<TestComponent>(cs))
            {
                if (ent.ID == 0)
                    Assert.True(comp.Value == 5);
                if (ent.ID == 1)
                    Assert.True(comp.Value == 6);
                numFound++;
            }
            Assert.Equal(2, numFound);
        }
    }
}
