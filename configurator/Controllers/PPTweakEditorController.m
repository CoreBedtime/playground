//Created by Salty on 9/2/26.

#import "PPTweakEditorController.h"

typedef enum : NSUInteger {
    Blacklist,
    Whitelist,
    FrameworkDependencies,
} TweakDictionaryParts;

@interface PPTweakEditorController ()
@property (strong) NSMutableDictionary *internalTweakOptions;
@property (strong) NSString *optionsPath;
@end

@implementation PPTweakEditorController
- (instancetype)initWithTitle:(NSString *)tweakName
              optionsDictPath:(NSString *)optionsPath
             tweakOptionsDict:(NSDictionary *)options{
    self = [super initWithNibName:@"PPTweakEditorPanel"
                           bundle:nil];
    if (self){
        _internalTweakOptions = [NSMutableDictionary dictionary];
        [_internalTweakOptions addEntriesFromDictionary:options];
        _optionsPath = optionsPath;
        [self.view.window setTitle:tweakName];
        
        for (NSTableView *v in @[_blacklistTableView, _whitelistTableView, _frameworkDependenciesTableView])
            [v reloadData];
    }
    return self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView{
    TweakDictionaryParts part = [[tableView identifier] isEqualToString:@"f"] ?
                                    FrameworkDependencies :
                                        [[tableView identifier] isEqualToString:@"b"] ?
                                            Blacklist :
                                                Whitelist;
    
    NSUInteger returnCount = 0;
    switch (part) {
        case Blacklist: ;
            returnCount = [[_internalTweakOptions objectForKey:@"blacklistedApps"] count] + 1;
            break;
        case Whitelist: ;
            returnCount = [[_internalTweakOptions objectForKey:@"whitelistedApps"] count] + 1;
            break;
        case FrameworkDependencies: ;
            returnCount = [[_internalTweakOptions objectForKey:@"frameworkDependencies"] count] + 1;
            break;
        default:
            break;
    }
    
    return returnCount;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row{
    TweakDictionaryParts part = [[tableView identifier] isEqualToString:@"f"] ?
                                    FrameworkDependencies :
                                        [[tableView identifier] isEqualToString:@"b"] ?
                                            Blacklist :
                                                Whitelist;

    NSArray<NSString *> *valuesFromDict = nil;
    
    if (part == Blacklist){
        valuesFromDict = [_internalTweakOptions objectForKey:@"blacklistedApps"];
    } else if (part == Whitelist){
        valuesFromDict = [_internalTweakOptions objectForKey:@"whitelistedApps"];
    } else if (part == FrameworkDependencies){
        valuesFromDict = [_internalTweakOptions objectForKey:@"frameworkDependencies"];
    }
    
    if (row >= [valuesFromDict count])
        return @"";
    
    NSString *ret = [valuesFromDict objectAtIndex:row];
    
    return ret;
}

- (void)tableView:(NSTableView *)tableView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row{
    TweakDictionaryParts part = [[tableView identifier] isEqualToString:@"f"] ?
                                    FrameworkDependencies :
                                        [[tableView identifier] isEqualToString:@"b"] ?
                                            Blacklist :
                                                Whitelist;
    
    if (![object isKindOfClass:[NSString class]])
        return;
    
    NSString *newValue = object;
    
    if (!self.internalTweakOptions)
        return;
    
    NSMutableArray<NSString *> *valuesFromDict = nil;
    
    if (part == Blacklist){
        valuesFromDict = [[_internalTweakOptions objectForKey:@"blacklistedApps"] mutableCopy] ?: [NSMutableArray array];
    } else if (part == Whitelist){
        valuesFromDict = [[_internalTweakOptions objectForKey:@"whitelistedApps"] mutableCopy] ?: [NSMutableArray array];
    } else if (part == FrameworkDependencies){
        valuesFromDict = [[_internalTweakOptions objectForKey:@"frameworkDependencies"] mutableCopy] ?: [NSMutableArray array];
    }
    
    if ([newValue isEqualToString:@""]){
        [valuesFromDict removeObjectAtIndex:row];
        [_blacklistTableView reloadData];
        [_whitelistTableView reloadData];
        [_frameworkDependenciesTableView reloadData];
        
        if (part == Blacklist){
            [self.internalTweakOptions setObject:valuesFromDict forKey:@"blacklistedApps"];
        } else if (part == Whitelist){
            [self.internalTweakOptions setObject:valuesFromDict forKey:@"whitelistedApps"];
        } else if (part == FrameworkDependencies){
            [self.internalTweakOptions setObject:valuesFromDict forKey:@"frameworkDependencies"];
        }
        return;
    }
    
    [valuesFromDict setObject:newValue atIndexedSubscript:row];
    
    if (part == Blacklist){
        [self.internalTweakOptions setObject:valuesFromDict forKey:@"blacklistedApps"];
    } else if (part == Whitelist){
        [self.internalTweakOptions setObject:valuesFromDict forKey:@"whitelistedApps"];
    } else if (part == FrameworkDependencies){
        [self.internalTweakOptions setObject:valuesFromDict forKey:@"frameworkDependencies"];
    }
}

- (IBAction)saveChanges:(id)sender{
    [self.internalTweakOptions writeToFile:self.optionsPath
                                atomically:YES];
}
@end
