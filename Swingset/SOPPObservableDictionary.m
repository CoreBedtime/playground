//Created by Salty on 8/26/26.

#import "SOPPObservableDictionary.h"

@interface SOPPObservableDictionary ()
@property (nonatomic, strong) NSMutableDictionary *internalStorage;
@end

@implementation SOPPObservableDictionary

- (instancetype)initWithDelegate:(id<SOPPObservableDictionaryDelegate>)delegate {
    if (self = [super init]) {
        _delegate = delegate;
        _internalStorage = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void)setObject:(id)anObject forKey:(id<NSCopying>)aKey {
    if (!anObject || !aKey) return;
    
    @synchronized (self) {
        if ([self.delegate respondsToSelector:@selector(dictionary:objectWithKey:willGetSetTo:)])
            [self.delegate dictionary:self objectWithKey:aKey willGetSetTo:anObject];
        
        [self.internalStorage setObject:anObject forKey:aKey];
        [self.delegate dictionary:self objectWithKey:aKey didGetSetTo:anObject];
    }
}

- (void)removeObjectForKey:(id)aKey {
    if (!aKey) return;

    @synchronized (self) {
        id removedObject = [self.internalStorage objectForKey:aKey];
        if (removedObject) {
            [self.delegate dictionary:self willRemoveObject:removedObject forKey:aKey];
            [self.internalStorage removeObjectForKey:aKey];
            if ([self.delegate respondsToSelector:@selector(dictionary:didRemoveObjectWithKey:)])
                [self.delegate dictionary:self didRemoveObjectWithKey:aKey];
        }
    }
}

- (id)objectForKey:(id)aKey {
    if (!aKey) return nil;
    
    @synchronized (self) {
        return [self.internalStorage objectForKey:aKey];
    }
}

- (void)removeAllObjects{
    for (id key in self.internalStorage.allKeys){
        [self removeObjectForKey:key];
    }
}

- (NSUInteger)count{
    return self.internalStorage.count;
}

- (NSArray *)allKeys{
    return [self.internalStorage allKeys];
}

- (void)setExistingKey:(id)aKey toNewKey:(id)aNewKey{
    @synchronized (self) {
        id origValue = [self.internalStorage valueForKey:aKey];
        [self.internalStorage setObject:origValue forKey:aNewKey];
        [self.internalStorage removeObjectForKey:aKey];
    }
}
@end
