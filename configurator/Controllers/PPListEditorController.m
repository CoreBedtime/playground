//Created by Salty on 8/29/26.

#import "PPListEditorController.h"

#import "PPMainViewController.h"

@interface PPListEditorController ()
@property (strong) NSMutableArray<NSString *> *internalStorage;
@end

@implementation PPListEditorController
- (instancetype)initWithDelegate:(PPMainViewController *)delegate{
    self = [super initWithNibName:@"PPListEditorPanel"
                           bundle:nil];
    if (self){
        _delegate = delegate;
        _internalStorage = [NSMutableArray array];
    }
    return self;
}

- (void)presentWithInitialArray:(NSArray<NSString *> *)array {
    if (!self.internalStorage)
        return;
    
    [self.internalStorage removeAllObjects];
    
    [self.internalStorage addObjectsFromArray:array];
    
    [[self.view window] makeKeyAndOrderFront:nil];
    
    [self.tableView reloadData];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView{
    return [self.internalStorage count] + 1 ?: 1;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row{
    if (row == [self.internalStorage count])
        return @"";
    
    return [self.internalStorage objectAtIndex:row];
}

- (void)tableView:(NSTableView *)tableView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row{
    if ([object isEqualToString:@""]){
        [self.internalStorage removeObjectAtIndex:row];
        [self.tableView reloadData];
        return;
    }
    
    [self.internalStorage setObject:object atIndexedSubscript:row];
    [self.tableView reloadData];
}

- (IBAction)saveChanges:(id)sender{
    [self.delegate updateGlobalBlacklistWithArray:[self.internalStorage copy]];
}
@end
